use snow_shot_http_services::TranslateService;
use tauri::State;

#[tauri::command]
pub async fn translate_text(
    translate_service: State<'_, TranslateService>,
    text: String,
    source_lang: String,
    target_lang: String,
) -> Result<snow_shot_http_services::TranslateResult, String> {
    let result = translate_service
        .translate(&text, &source_lang, &target_lang)
        .await;
    Ok(result)
}