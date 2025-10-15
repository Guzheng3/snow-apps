use tauri::command;
use tokio::sync::Mutex;

use serde::{Deserialize, Serialize};

#[derive(Clone, Serialize, Deserialize)]
pub struct CaptureState {
    pub capturing: bool,
}

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
