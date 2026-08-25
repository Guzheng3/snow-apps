use crate::cloud_translate::{CloudConfig, CloudEngine, CloudTranslator};
use std::sync::Arc;
use tauri::command;
use tokio::sync::Mutex;

/// Global cloud translator state
pub struct CloudTranslatorState {
    pub translator: Arc<Mutex<Option<CloudTranslator>>>,
    pub config: Arc<Mutex<CloudConfig>>,
}

impl CloudTranslatorState {
    pub fn new() -> Self {
        let config = CloudConfig::default();
        let translator = CloudTranslator::new(config.clone());
        Self {
            translator: Arc::new(Mutex::new(Some(translator))),
            config: Arc::new(Mutex::new(config)),
        }
    }
}

#[command]
pub async fn cloud_translate(
    state: tauri::State<'_, CloudTranslatorState>,
    text: String,
    from: String,
    to: String,
) -> Result<String, String> {
    let translator = state.translator.lock().await;
    let t = translator
        .as_ref()
        .ok_or("CloudTranslator not initialized")?;
    t.translate(&text, &from, &to).await
}

#[command]
pub async fn cloud_translate_set_config(
    state: tauri::State<'_, CloudTranslatorState>,
    config_json: String,
) -> Result<(), String> {
    let config: CloudConfig = serde_json::from_str(&config_json)
        .map_err(|e| format!("Invalid config JSON: {e}"))?;
    let mut cfg = state.config.lock().await;
    *cfg = config.clone();
    let mut t = state.translator.lock().await;
    *t = Some(CloudTranslator::new(config));
    log::info!("[cloud_translate] config updated");
    Ok(())
}

#[command]
pub async fn cloud_translate_get_engines() -> Result<Vec<serde_json::Value>, String> {
    let engines = vec![
        CloudEngine::Microsoft,
        CloudEngine::ICiba,
        CloudEngine::Transmart,
        CloudEngine::Yandex,
        CloudEngine::Baidu,
        CloudEngine::BigModel,
    ];
    let result: Vec<serde_json::Value> = engines
        .iter()
        .map(|e| {
            serde_json::json!({
                "id": format!("{:?}", e).to_lowercase(),
                "name": e.name(),
                "needsKey": e.needs_key(),
                "isFree": e.is_free(),
            })
        })
        .collect();
    Ok(result)
}

#[command]
pub async fn cloud_translate_test(
    state: tauri::State<'_, CloudTranslatorState>,
    engine_id: String,
    text: String,
    from: String,
    to: String,
) -> Result<String, String> {
    let engine: CloudEngine = match engine_id.as_str() {
        "microsoft" => CloudEngine::Microsoft,
        "iciba" => CloudEngine::ICiba,
        "transmart" => CloudEngine::Transmart,
        "yandex" => CloudEngine::Yandex,
        "baidu" => CloudEngine::Baidu,
        "bigmodel" => CloudEngine::BigModel,
        _ => return Err(format!("Unknown engine: {engine_id}")),
    };
    let translator = state.translator.lock().await;
    let t = translator
        .as_ref()
        .ok_or("CloudTranslator not initialized")?;
    t.test_engine(engine, &text, &from, &to).await
}