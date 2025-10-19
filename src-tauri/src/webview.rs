use snow_shot_webview::create_shared_buffer;
use tauri::command;
use tokio::sync::Mutex;

#[command]
pub async fn create_webview_shared_buffer(
    webview: tauri::Webview,
    data: Vec<u8>,
    transfer_type: String,
) -> Result<(), String> {
    create_shared_buffer(webview, &data, &[], transfer_type).await?;

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

#[cfg(target_os = "windows")]
#[command]
pub async fn create_webview_shared_buffer_channel(
    support_webview_shared_buffer: tauri::State<'_, Mutex<bool>>,
    shared_buffer_service: tauri::State<'_, std::sync::Arc<snow_shot_webview::SharedBufferService>>,
    webview: tauri::Webview,
    channel_id: String,
    data_size: usize,
) -> Result<bool, String> {
    if !*support_webview_shared_buffer.lock().await {
        return Ok(false);
    }

    match shared_buffer_service.create_channel(channel_id, webview, data_size) {
        Ok(_) => Ok(true),
        Err(e) => Err(e),
    }
}
