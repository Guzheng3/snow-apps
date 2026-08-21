use std::path::PathBuf;

use snow_shot_app_services::translate_service::TranslateService;
use tauri::command;
use tokio::sync::Mutex;

#[command]
pub async fn translate_local_init(
    translate_instance: tauri::State<'_, Mutex<TranslateService>>,
    model_dir: PathBuf,
) -> Result<(), String> {
    let mut service = translate_instance.lock().await;
    service.init(&model_dir).await
}

#[command]
pub async fn translate_local_release(
    translate_instance: tauri::State<'_, Mutex<TranslateService>>,
) -> Result<(), String> {
    let mut service = translate_instance.lock().await;
    service.release();
    Ok(())
}

#[command]
pub async fn translate_local_text(
    translate_instance: tauri::State<'_, Mutex<TranslateService>>,
    text: String,
) -> Result<String, String> {
    let mut service = translate_instance.lock().await;
    service.translate(&text).await
}
