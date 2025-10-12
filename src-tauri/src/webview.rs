use snow_shot_webview::create_shared_buffer;
use tauri::command;
use tokio::sync::Mutex;

#[command]
pub async fn create_webview_shared_buffer(
    webview: tauri::Webview,
    data: Vec<u8>,
) -> Result<(), String> {
    create_shared_buffer(webview, &data, &[]).await?;

    Ok(())
}

#[command]
pub async fn set_support_webview_shared_buffer(
    support_webview_shared_buffer: tauri::State<'_, Mutex<bool>>,
    value: bool,
) -> Result<(), String> {
    *support_webview_shared_buffer.lock().await = value;

    Ok(())
}
