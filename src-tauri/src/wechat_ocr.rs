use std::path::PathBuf;
use std::sync::Arc;

use tauri::command;
use tokio::sync::Mutex;

use snow_shot_app_services::wechat_ocr_service::WeChatOcrService;
use snow_shot_tauri_commands_ocr::OcrDetectResult;

/// Global WeChat OCR state
pub struct WeChatOcrState {
    pub service: Arc<Mutex<WeChatOcrService>>,
}

impl WeChatOcrState {
    pub fn new() -> Self {
        Self {
            service: Arc::new(Mutex::new(WeChatOcrService::new())),
        }
    }
}

#[command]
pub async fn wechat_ocr_init(
    state: tauri::State<'_, WeChatOcrState>,
    helper_path: String,
    wechat_dir: Option<String>,
) -> Result<(), String> {
    let mut service = state.service.lock().await;
    let helper = PathBuf::from(&helper_path);
    let wechat = wechat_dir.map(PathBuf::from);
    service.init(helper, wechat)
}

#[command]
pub async fn wechat_ocr_detect(
    state: tauri::State<'_, WeChatOcrState>,
    request: tauri::ipc::Request<'_>,
) -> Result<OcrDetectResult, String> {
    let image_data = match request.body() {
        tauri::ipc::InvokeBody::Raw(data) => data,
        _ => return Err("[wechat_ocr_detect] Invalid request body".to_string()),
    };

    let service = state.service.lock().await;
    let output = service.detect(image_data).await?;

    // Convert WeChatOcrOutput to OcrDetectResult
    let text_blocks = output
        .text_blocks
        .into_iter()
        .map(|block| paddle_ocr_rs::ocr_result::TextBlock {
            box_points: block
                .box_points
                .into_iter()
                .map(|p| paddle_ocr_rs::ocr_result::Point {
                    x: p.x as i32,
                    y: p.y as i32,
                })
                .collect(),
            text: block.text,
            text_score: block.text_score,
        })
        .collect();

    Ok(OcrDetectResult {
        text_blocks,
        scale_factor: output.scale_factor,
    })
}

#[command]
pub async fn wechat_ocr_release(
    state: tauri::State<'_, WeChatOcrState>,
) -> Result<(), String> {
    let mut service = state.service.lock().await;
    service.release();
    Ok(())
}