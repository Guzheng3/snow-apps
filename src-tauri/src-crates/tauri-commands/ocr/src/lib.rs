use paddle_ocr_rs::ocr_result::TextBlock;
use serde::Deserialize;
use serde::Serialize;
use snow_shot_app_services::ocr_service::{OcrModel, OcrService};
use std::io::Cursor;
use tokio::sync::Mutex;

pub async fn ocr_init(
    app: tauri::AppHandle,
    ocr_service: tauri::State<'_, Mutex<OcrService>>,
    model: OcrModel,
) -> Result<(), String> {
    let mut ocr_service = ocr_service.lock().await;

    ocr_service.init_models(app, model)?;

    Ok(())
}

#[derive(Serialize, Deserialize)]
pub struct OcrDetectResult {
    pub text_blocks: Vec<TextBlock>,
    pub scale_factor: f32,
}

pub async fn ocr_detect(
    ocr_service: tauri::State<'_, Mutex<OcrService>>,
    request: tauri::ipc::Request<'_>,
) -> Result<OcrDetectResult, String> {
    let mut ocr_service = ocr_service.lock().await;

    let image_data = match request.body() {
        tauri::ipc::InvokeBody::Raw(data) => data,
        _ => return Err("[ocr_detect] Invalid request body".to_string()),
    };

    let mut image = match image::load(Cursor::new(image_data), image::ImageFormat::Png) {
        Ok(image) => image,
        Err(_) => return Err("[ocr_detect] Invalid image".to_string()),
    };

    let mut scale_factor: f32 = match request.headers().get("x-scale-factor") {
        Some(header) => match header.to_str() {
            Ok(scale_factor) => scale_factor.parse::<f32>().unwrap(),
            Err(_) => return Err("[ocr_detect] Invalid scale factor".to_string()),
        },
        None => return Err("[ocr_detect] Missing scale factor".to_string()),
    };

    // 分辨率过小的图片识别可能有问题，当 scale_factor 低于 1.5 时，放大图片使有效缩放达到 1.5
    let target_scale_factor = 1.5;
    if scale_factor < target_scale_factor && scale_factor > 0.0 {
        scale_factor = target_scale_factor;
        let resize_factor = target_scale_factor / scale_factor;
        image = image.resize(
            (image.width() as f32 * resize_factor) as u32,
            (image.height() as f32 * resize_factor) as u32,
            image::imageops::FilterType::Lanczos3,
        );
    }

    let detect_angle = match request.headers().get("x-detect-angle") {
        Some(header) => match header.to_str() {
            Ok(detect_angle) => detect_angle.parse::<bool>().unwrap(),
            Err(_) => return Err("[ocr_detect] Invalid detect angle".to_string()),
        },
        None => return Err("[ocr_detect] Missing detect angle".to_string()),
    };

    let image_buffer = image.to_rgb8();
    let ocr_result = ocr_service.get_session().detect_angle_rollback(
        &image_buffer,
        50,
        image.height().max(image.width()),
        0.5,
        0.3,
        1.6,
        detect_angle,
        false,
        0.9, // 屏幕截取的文字质量通常较高，且非横向排版的情况较少，尽量减少角度的影响
    );

    match ocr_result {
        Ok(ocr_result) => Ok(OcrDetectResult {
            text_blocks: ocr_result.text_blocks,
            scale_factor,
        }),
        Err(e) => return Err(format!("[ocr_detect] Failed to detect text: {}", e)),
    }
}

pub async fn ocr_release(ocr_service: tauri::State<'_, Mutex<OcrService>>) -> Result<(), String> {
    let mut ocr_service = ocr_service.lock().await;

    ocr_service.release_session()?;

    Ok(())
}
