use std::path::PathBuf;

use tauri::command;
use tokio::sync::Mutex;

use snow_shot_app_services::ocr_service::{OcrModel, OcrService};
use snow_shot_tauri_commands_ocr::OcrDetectResult;

#[command]
pub async fn ocr_init(
    ocr_instance: tauri::State<'_, Mutex<OcrService>>,
    orc_plugin_path: PathBuf,
    model: OcrModel,
    hot_start: bool,
    model_write_to_memory: bool,
) -> Result<(), String> {
    snow_shot_tauri_commands_ocr::ocr_init(
        orc_plugin_path,
        ocr_instance,
        model,
        hot_start,
        model_write_to_memory,
    )
    .await
}

#[command]
pub async fn ocr_detect(
    ocr_instance: tauri::State<'_, Mutex<OcrService>>,
    request: tauri::ipc::Request<'_>,
) -> Result<OcrDetectResult, String> {
    snow_shot_tauri_commands_ocr::ocr_detect(ocr_instance, request).await
}

#[command]
pub async fn ocr_release(ocr_instance: tauri::State<'_, Mutex<OcrService>>) -> Result<(), String> {
    snow_shot_tauri_commands_ocr::ocr_release(ocr_instance).await
}
