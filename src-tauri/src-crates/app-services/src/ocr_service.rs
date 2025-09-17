use num_cpus;
use paddle_ocr_rs::ocr_lite::OcrLite;
use serde::{Deserialize, Serialize};
use tauri::{Manager, path::BaseDirectory};

pub struct OcrService {
    ocr_core: OcrLite,
    det_model: Option<Vec<u8>>,
    rec_model: Option<Vec<u8>>,
    cls_model: Option<Vec<u8>>,
}

#[derive(Debug, Clone, PartialEq, Eq, Hash, Copy, PartialOrd, Serialize, Deserialize)]
pub enum OcrModel {
    RapidOcrV4,
    RapidOcrV5,
}

impl OcrService {
    pub fn new() -> Self {
        Self {
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
                    .expect("[OcrService::init_ocr_core] Det model is not loaded"),
                self.cls_model
                    .as_ref()
                    .expect("[OcrService::init_ocr_core] Cls model is not loaded"),
                self.rec_model
                    .as_ref()
                    .expect("[OcrService::init_ocr_core] Rec model is not loaded"),
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
        app: tauri::AppHandle,
        model: OcrModel,
    ) -> Result<(), String> {
        let resource_path = match app.path().resolve("models", BaseDirectory::Resource) {
            Ok(resource_path) => resource_path,
            Err(_) => {
                return Err(format!(
                    "[OcrService::init_models] Failed to get resource path"
                ));
            }
        };

        // 加载模型到内存
        let (det_model_path, cls_model_path, rec_model_path) = match model {
            OcrModel::RapidOcrV4 => (
                resource_path.join("paddle_ocr/ch_PP-OCRv4_det_infer.onnx"),
                resource_path.join("paddle_ocr/ch_ppocr_mobile_v2.0_cls_infer.onnx"),
                resource_path.join("paddle_ocr/ch_PP-OCRv4_rec_infer.onnx"),
            ),
            OcrModel::RapidOcrV5 => (
                resource_path.join("paddle_ocr/ch_PP-OCRv4_det_infer.onnx"),
                resource_path.join("paddle_ocr/ch_ppocr_mobile_v2.0_cls_infer.onnx"),
                resource_path.join("paddle_ocr/ch_PP-OCRv5_rec_mobile_infer.onnx"),
            ),
        };

        self.det_model =
            Some(tokio::fs::read(det_model_path).await.map_err(|e| {
                format!("[OcrService::init_models] Failed to read det model: {}", e)
            })?);
        self.cls_model =
            Some(tokio::fs::read(cls_model_path).await.map_err(|e| {
                format!("[OcrService::init_models] Failed to read cls model: {}", e)
            })?);
        self.rec_model =
            Some(tokio::fs::read(rec_model_path).await.map_err(|e| {
                format!("[OcrService::init_models] Failed to read rec model: {}", e)
            })?);

        // 初始化 onnx session
        self.init_session()?;

        Ok(())
    }

    /// 释放 onnx session，并初始化新的 session
    pub fn release_session(&mut self) -> Result<(), String> {
        self.init_session()
    }

    pub fn get_session(&mut self) -> &mut OcrLite {
        &mut self.ocr_core
    }
}
