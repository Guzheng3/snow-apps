use std::path::{Path, PathBuf};
use std::sync::Arc;

use ort::session::Session;
use ort::value::Tensor;
use tokio::sync::Mutex;

/// 本地翻译服务：使用 onnxruntime 加载 Opus-MT（Helsinki-NLP）中英翻译模型
/// 模型文件（从插件目录加载，由插件系统按需下载）：
///   - encoder_model.onnx
///   - decoder_model.onnx
///   - tokenizer.json
pub struct TranslateService {
    encoder_session: Option<Session>,
    decoder_session: Option<Session>,
    tokenizer: Option<Arc<Mutex<tokenizers::Tokenizer>>>,
    /// 已加载的模型目录（用于判断是否需要重新加载）
    loaded_dir: Option<PathBuf>,
}

impl TranslateService {
    pub fn new() -> Self {
        Self {
            encoder_session: None,
            decoder_session: None,
            tokenizer: None,
            loaded_dir: None,
        }
    }

    /// 从插件目录加载模型（如果目录变化则重新加载）
    pub async fn init(&mut self, model_dir: &Path) -> Result<(), String> {
        if self.loaded_dir.as_deref() == Some(model_dir) && self.encoder_session.is_some() {
            return Ok(());
        }

        let encoder_path = model_dir.join("encoder_model.onnx");
        let decoder_path = model_dir.join("decoder_model.onnx");
        let tokenizer_path = model_dir.join("tokenizer.json");

        if !encoder_path.exists() || !decoder_path.exists() || !tokenizer_path.exists() {
            return Err(format!(
                "[TranslateService::init] model files not found in {:?} (need encoder_model.onnx, decoder_model.onnx, tokenizer.json)",
                model_dir
            ));
        }

        let encoder = Session::builder()
            .map_err(|e| format!("[TranslateService] encoder builder error: {e}"))?
            .with_intra_threads(num_cpus::get_physical())
            .map_err(|e| format!("[TranslateService] encoder threads error: {e}"))?
            .with_optimization_level(ort::session::builder::GraphOptimizationLevel::Level3)
            .map_err(|e| format!("[TranslateService] encoder opt error: {e}"))?
            .commit_from_file(&encoder_path)
            .map_err(|e| format!("[TranslateService] encoder load error: {e}"))?;
        let decoder = Session::builder()
            .map_err(|e| format!("[TranslateService] decoder builder error: {e}"))?
            .with_intra_threads(num_cpus::get_physical())
            .map_err(|e| format!("[TranslateService] decoder threads error: {e}"))?
            .with_optimization_level(ort::session::builder::GraphOptimizationLevel::Level3)
            .map_err(|e| format!("[TranslateService] decoder opt error: {e}"))?
            .commit_from_file(&decoder_path)
            .map_err(|e| format!("[TranslateService] decoder load error: {e}"))?;
        let tokenizer = tokenizers::Tokenizer::from_file(&tokenizer_path)
            .map_err(|e| format!("[TranslateService] tokenizer load error: {e}"))?;

        self.encoder_session = Some(encoder);
        self.decoder_session = Some(decoder);
        self.tokenizer = Some(Arc::new(Mutex::new(tokenizer)));
        self.loaded_dir = Some(model_dir.to_path_buf());

        log::info!("[TranslateService::init] loaded model from {:?}", model_dir);
        Ok(())
    }

    /// 释放模型
    pub fn release(&mut self) {
        self.encoder_session = None;
        self.decoder_session = None;
        self.tokenizer = None;
        self.loaded_dir = None;
    }

    /// 翻译一段文本（中英，greedy 解码）
    pub async fn translate(&mut self, text: &str) -> Result<String, String> {
        let encoder = self
            .encoder_session
            .as_ref()
            .ok_or("[TranslateService] encoder not initialized")?;
        let decoder = self
            .decoder_session
            .as_ref()
            .ok_or("[TranslateService] decoder not initialized")?;
        let tokenizer = self
            .tokenizer
            .as_ref()
            .ok_or("[TranslateService] tokenizer not initialized")?;

        let tokenizer_guard = tokenizer.lock().await;
        let encoding = tokenizer_guard
            .encode(text, true)
            .map_err(|e| format!("[TranslateService] tokenize error: {e}"))?;
        let input_ids: Vec<i64> = encoding.get_ids().iter().map(|&x| x as i64).collect();
        let attention_mask: Vec<i64> = encoding
            .get_attention_mask()
            .iter()
            .map(|&x| x as i64)
            .collect();

        // encoder 输入：shape [1, seq_len]
        let enc_shape = vec![1i64, input_ids.len() as i64];
        let input_ids_tensor = Tensor::from_array((enc_shape.clone(), input_ids))
            .map_err(|e| format!("[TranslateService] input_ids tensor error: {e}"))?;
        let attention_mask_tensor = Tensor::from_array((enc_shape, attention_mask))
            .map_err(|e| format!("[TranslateService] attention_mask tensor error: {e}"))?;

        // encoder 前向（DynValue 不能 clone，用 try_extract_tensor 提取数据）
        let encoder_outputs = encoder
            .run(ort::inputs![input_ids_tensor, attention_mask_tensor])
            .map_err(|e| format!("[TranslateService] encoder run error: {e}"))?;
        let (enc_shape_ref, enc_data) = encoder_outputs[0]
            .try_extract_tensor::<f32>()
            .map_err(|e| format!("[TranslateService] encoder hidden extract error: {e}"))?;
        let encoder_hidden_data: Vec<f32> = enc_data.to_vec();
        let encoder_hidden_shape: Vec<i64> = enc_shape_ref.iter().map(|&d| d).collect();

        // decoder 自回归（greedy）
        let bos_id = 0i64;
        let eos_id = 2i64;
        let max_len = 512usize;
        let mut generated: Vec<i64> = vec![bos_id];

        for _ in 0..max_len {
            let dec_shape = vec![1i64, generated.len() as i64];
            let decoder_input_ids = Tensor::from_array((dec_shape, generated.clone()))
                .map_err(|e| format!("[TranslateService] decoder input tensor error: {e}"))?;
            let encoder_hidden_tensor =
                Tensor::from_array((encoder_hidden_shape.clone(), encoder_hidden_data.clone()))
                    .map_err(|e| {
                        format!("[TranslateService] encoder hidden tensor error: {e}")
                    })?;

            let decoder_outputs = decoder
                .run(ort::inputs![decoder_input_ids, encoder_hidden_tensor])
                .map_err(|e| format!("[TranslateService] decoder run error: {e}"))?;
            let (logits_shape, logits_data) = decoder_outputs[0]
                .try_extract_tensor::<f32>()
                .map_err(|e| format!("[TranslateService] logits extract error: {e}"))?;

            // logits shape: [1, seq_len, vocab]
            let seq_len = generated.len();
            let total = logits_shape.num_elements();
            if seq_len == 0 || total % seq_len != 0 {
                return Err(format!(
                    "[TranslateService] unexpected logits shape: num_elements={total}, seq_len={seq_len}"
                ));
            }
            let vocab = total / seq_len;
            let offset = (seq_len - 1) * vocab;
            if offset + vocab > logits_data.len() {
                return Err(format!(
                    "[TranslateService] logits data too short: offset={offset}, vocab={vocab}, len={}",
                    logits_data.len()
                ));
            }
            let last_row = &logits_data[offset..offset + vocab];

            // argmax
            let mut best_id = 0usize;
            let mut best_val = f32::NEG_INFINITY;
            for (i, v) in last_row.iter().enumerate() {
                if *v > best_val {
                    best_val = *v;
                    best_id = i;
                }
            }
            let next_id = best_id as i64;
            if next_id == eos_id {
                break;
            }
            generated.push(next_id);
            if generated.len() >= max_len {
                break;
            }
        }

        // 解码（跳过 bos）
        let decoded = tokenizer_guard
            .decode(
                &generated[1..].iter().map(|&x| x as u32).collect::<Vec<u32>>(),
                true,
            )
            .map_err(|e| format!("[TranslateService] decode error: {e}"))?;

        Ok(decoded)
    }
}
