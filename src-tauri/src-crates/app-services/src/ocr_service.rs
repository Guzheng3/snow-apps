use std::path::PathBuf;

use num_cpus;
use paddle_ocr_rs::ocr_lite::OcrLite;
use serde::{Deserialize, Serialize};

pub struct OcrService {
    has_init_models: bool,
    ocr_core: OcrLite,
    det_model: Option<(PathBuf, Vec<u8>)>,
    rec_model: Option<(PathBuf, Vec<u8>)>,
    cls_model: Option<(PathBuf, Vec<u8>)>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Copy, PartialOrd, Serialize, Deserialize)]
pub enum OcrModel {
    RapidOcrV4,
    RapidOcrV5,
}

impl OcrService {
    pub fn new() -> Self {
        Self {
            has_init_models: false,
            ocr_core: OcrLite::new(),
            det_model: None,
            rec_model: None,
            cls_model: None,
        }
    }

    pub fn init_session(&mut self) -> Result<(), String> {
        self.ocr_core
            .init_models_from_memory_custom(
                self.det_model
                    .as_ref()
                    .expect("[OcrService::init_ocr_core] Det model is not loaded")
                    .1
                    .as_ref(),
                self.cls_model
                    .as_ref()
                    .expect("[OcrService::init_ocr_core] Cls model is not loaded")
                    .1
                    .as_ref(),
                self.rec_model
                    .as_ref()
                    .expect("[OcrService::init_ocr_core] Rec model is not loaded")
                    .1
                    .as_ref(),
                |builder| {
                    let num_thread = num_cpus::get_physical();
                    Ok(builder
                        .with_inter_threads(num_thread)?
                        .with_intra_threads(num_thread)?
                        .with_optimization_level(
                            ort::session::builder::GraphOptimizationLevel::Level3,
                        )?)
                },
            )
            .expect("[OcrService::init_ocr_core] Failed to init models");

        Ok(())
    }

    pub async fn init_models(
        &mut self,
        orc_plugin_path: PathBuf,
        model: OcrModel,
    ) -> Result<(), String> {
        log::info!(
            "[OcrService::init_models] orc_plugin_path: {:?}, model: {:?}",
            orc_plugin_path,
            model
        );

        // 加载模型到内存
        let (det_model_path, cls_model_path, rec_model_path) = match model {
            OcrModel::RapidOcrV4 => (
                orc_plugin_path.join("ch_PP-OCRv4_det_infer.onnx"),
                orc_plugin_path.join("ch_ppocr_mobile_v2.0_cls_infer.onnx"),
                orc_plugin_path.join("ch_PP-OCRv4_rec_infer.onnx"),
            ),
            OcrModel::RapidOcrV5 => (
                orc_plugin_path.join("ch_PP-OCRv4_det_infer.onnx"),
                orc_plugin_path.join("ch_ppocr_mobile_v2.0_cls_infer.onnx"),
                orc_plugin_path.join("ch_PP-OCRv5_rec_mobile_infer.onnx"),
            ),
        };

        if self.det_model.is_some()
            && self.cls_model.is_some()
            && self.rec_model.is_some()
            && self.det_model.as_ref().unwrap().0 == det_model_path
            && self.cls_model.as_ref().unwrap().0 == cls_model_path
            && self.rec_model.as_ref().unwrap().0 == rec_model_path
        {
            log::info!("[OcrService::init_models] Models already loaded");
            return Ok(());
        }

        let (det_result, cls_result, rec_result) = tokio::join!(
            tokio::fs::read(det_model_path.clone()),
            tokio::fs::read(cls_model_path.clone()),
            tokio::fs::read(rec_model_path.clone())
        );

        self.det_model = Some((
            det_model_path,
            det_result.map_err(|e| {
                format!("[OcrService::init_models] Failed to read det model: {}", e)
            })?,
        ));
        self.cls_model = Some((
            cls_model_path,
            cls_result.map_err(|e| {
                format!("[OcrService::init_models] Failed to read cls model: {}", e)
            })?,
        ));
        self.rec_model = Some((
            rec_model_path,
            rec_result.map_err(|e| {
                format!("[OcrService::init_models] Failed to read rec model: {}", e)
            })?,
        ));

        // 初始化 onnx session
        self.init_session()?;

        self.has_init_models = true;

        Ok(())
    }

    /// 释放 onnx session，并初始化新的 session
    pub fn release_session(&mut self) -> Result<(), String> {
        self.init_session()
    }

    pub fn get_session(&mut self) -> Result<&mut OcrLite, String> {
        if !self.has_init_models {
            return Err(
                "[OcrService::get_session] Not init models, please init models first".to_string(),
            );
        }

        Ok(&mut self.ocr_core)
    }
}
