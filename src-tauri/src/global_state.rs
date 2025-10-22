use tauri::command;
use tokio::sync::Mutex;

use snow_shot_global_state::CaptureState;

#[command]
pub async fn set_capture_state(
    capture_state: tauri::State<'_, Mutex<CaptureState>>,
    capturing: bool,
) -> Result<(), String> {
    let mut capture_state = capture_state.lock().await;
    capture_state.capturing = capturing;

    Ok(())
}

#[command]
pub async fn get_capture_state(
    capture_state: tauri::State<'_, Mutex<CaptureState>>,
) -> Result<CaptureState, String> {
    let capture_state = capture_state.lock().await;
    Ok(capture_state.clone())
}
