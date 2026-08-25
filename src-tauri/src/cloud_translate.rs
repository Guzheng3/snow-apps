//! Cloud translation engines: Microsoft, ICiba, Transmart, Yandex, Baidu, BigModel (智谱)
//! All engines are built-in, no plugin dependency.
use std::time::{SystemTime, UNIX_EPOCH};

use reqwest::Client;
use serde::{Deserialize, Serialize};
use md5::{Digest, Md5};
use serde_json::Value;
use base64::Engine;
use tokio::sync::Mutex;

/// Supported cloud translation engines
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CloudEngine {
    #[serde(rename = "microsoft")]
    Microsoft,
    #[serde(rename = "iciba")]
    ICiba,
    #[serde(rename = "transmart")]
    Transmart,
    #[serde(rename = "yandex")]
    Yandex,
    #[serde(rename = "baidu")]
    Baidu,
    #[serde(rename = "bigmodel")]
    BigModel,
}

impl CloudEngine {
    pub fn name(&self) -> &str {
        match self {
            CloudEngine::Microsoft => "微软翻译",
            CloudEngine::ICiba => "金山词霸",
            CloudEngine::Transmart => "腾讯通天塔",
            CloudEngine::Yandex => "Yandex",
            CloudEngine::Baidu => "百度翻译",
            CloudEngine::BigModel => "智谱GLM",
        }
    }

    pub fn needs_key(&self) -> bool {
        matches!(self, CloudEngine::Baidu | CloudEngine::BigModel)
    }

    pub fn is_free(&self) -> bool {
        matches!(
            self,
            CloudEngine::Microsoft
                | CloudEngine::ICiba
                | CloudEngine::Transmart
                | CloudEngine::Yandex
        )
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CloudConfig {
    pub baidu_app_id: String,
    pub baidu_app_key: String,
    pub bigmodel_key: String,
    pub engine_order: Vec<CloudEngine>,
}

impl Default for CloudConfig {
    fn default() -> Self {
        Self {
            baidu_app_id: String::new(),
            baidu_app_key: String::new(),
            bigmodel_key: String::new(),
            engine_order: vec![
                CloudEngine::Microsoft,
                CloudEngine::Transmart,
                CloudEngine::ICiba,
                CloudEngine::Yandex,
                CloudEngine::Baidu,
                CloudEngine::BigModel,
            ],
        }
    }
}

/// Cached Microsoft Edge token
struct EdgeTokenCache {
    token: String,
    expires_at: u64, // unix seconds
}

/// HTTP client wrapper
pub struct CloudTranslator {
    client: Client,
    config: CloudConfig,
    edge_token: Mutex<Option<EdgeTokenCache>>,
}

impl CloudTranslator {
    pub fn new(config: CloudConfig) -> Self {
        Self {
            client: Client::new(),
            config,
            edge_token: Mutex::new(None),
        }
    }

    /// Translate text using the configured engine order (fallback on failure)
    pub async fn translate(&self, text: &str, from: &str, to: &str) -> Result<String, String> {
        for engine in &self.config.engine_order {
            match self.try_engine(*engine, text, from, to).await {
                Ok(result) => return Ok(result),
                Err(e) => {
                    log::warn!("[CloudTranslate] {} failed: {}, trying next", engine.name(), e);
                    continue;
                }
            }
        }
        Err("所有翻译引擎均失败".to_string())
    }

    /// Test a single engine directly (no fallback)
    pub async fn test_engine(
        &self,
        engine: CloudEngine,
        text: &str,
        from: &str,
        to: &str,
    ) -> Result<String, String> {
        self.try_engine(engine, text, from, to).await
    }

    async fn try_engine(
        &self,
        engine: CloudEngine,
        text: &str,
        from: &str,
        to: &str,
    ) -> Result<String, String> {
        match engine {
            CloudEngine::Microsoft => self.translate_microsoft(text, from, to).await,
            CloudEngine::ICiba => self.translate_iciba(text, from, to).await,
            CloudEngine::Transmart => self.translate_transmart(text, from, to).await,
            CloudEngine::Yandex => self.translate_yandex(text, from, to).await,
            CloudEngine::Baidu => self.translate_baidu(text, from, to).await,
            CloudEngine::BigModel => self.translate_bigmodel(text, from, to).await,
        }
    }

    // ========== Microsoft 翻译 (Edge Token) - 免Key ==========
    async fn get_edge_token(&self) -> Result<String, String> {
        // Check cache first
        {
            let cache = self.edge_token.lock().await;
            if let Some(ref c) = *cache {
                let now = SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .unwrap()
                    .as_secs();
                // Refresh 1 minute before expiry
                if now < c.expires_at.saturating_sub(60) {
                    return Ok(c.token.clone());
                }
            }
        }

        // Fetch new token
        let resp = self
            .client
            .get("https://edge.microsoft.com/translate/auth")
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .send()
            .await
            .map_err(|e| format!("Microsoft auth HTTP error: {e}"))?;

        let token = resp
            .text()
            .await
            .map_err(|e| format!("Microsoft auth read error: {e}"))?
            .trim()
            .trim_matches('"')
            .to_string();

        if token.is_empty() {
            return Err("Microsoft: empty auth token".to_string());
        }

        // Parse JWT expiry (simple base64 decode of payload)
        let expires_at = Self::parse_jwt_expiry(&token).unwrap_or_else(|| {
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_secs()
                + 300 // fallback: 5 minutes
        });

        let mut cache = self.edge_token.lock().await;
        *cache = Some(EdgeTokenCache {
            token: token.clone(),
            expires_at,
        });

        Ok(token)
    }

    fn parse_jwt_expiry(token: &str) -> Option<u64> {
        let parts: Vec<&str> = token.split('.').collect();
        if parts.len() < 2 {
            return None;
        }
        let payload_b64 = parts[1].replace('-', "+").replace('_', "/");
        let padded = match payload_b64.len() % 4 {
            0 => payload_b64,
            n => payload_b64 + &"=".repeat(4 - n),
        };
        let bytes = base64::engine::general_purpose::STANDARD
            .decode(&padded)
            .ok()?;
        let json: Value = serde_json::from_slice(&bytes).ok()?;
        json["exp"].as_u64()
    }

    async fn translate_microsoft(
        &self,
        text: &str,
        from: &str,
        to: &str,
    ) -> Result<String, String> {
        let token = self.get_edge_token().await?;

        let mut url = format!(
            "https://api-edge.cognitive.microsofttranslator.com/translate?api-version=3.0&to={to}"
        );
        if from != "auto" && !from.is_empty() {
            url.push_str(&format!("&from={from}"));
        }

        let body = serde_json::json!([{"Text": text}]);

        let resp = self
            .client
            .post(&url)
            .header("Authorization", format!("Bearer {token}"))
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("Microsoft HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("Microsoft JSON error: {e}"))?;

        json.as_array()
            .and_then(|arr| arr.first())
            .and_then(|r| r["translations"].as_array())
            .and_then(|arr| arr.first())
            .and_then(|t| t["text"].as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| "Microsoft: empty result".to_string())
    }

    // ========== ICiba (金山词霸) - 免Key ==========
    async fn translate_iciba(&self, text: &str, from: &str, to: &str) -> Result<String, String> {
        let path = "/dictionary/fy/batch";
        let ts = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis()
            .to_string();
        let client = "6";
        let key = "1000006";
        let salt = "7ece94d9f9c202b0d2ec557dg4r9bc";
        let raw = format!("{path}{client}{key}{ts}{salt}");
        let mut hasher = Md5::new();
        hasher.update(raw.as_bytes());
        let sig = format!("{:x}", hasher.finalize());
        let url = format!(
            "https://dictionary.iciba.com/dictionary/fy/batch?client={client}&key={key}&timestamp={ts}&signature={sig}"
        );

        let body = serde_json::json!({
            "from": from,
            "to": to,
            "textList": [text]
        });

        let resp = self
            .client
            .post(&url)
            .header("Origin", "https://www.iciba.com")
            .header("Referer", "https://www.iciba.com/")
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("ICiba HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("ICiba JSON error: {e}"))?;

        if json["code"].as_i64() != Some(1) {
            return Err(format!("ICiba error: {}", json["msg"]));
        }
        let data = &json["data"];
        if let Some(arr) = data.as_array() {
            if let Some(first) = arr.first() {
                if let Some(out) = first["out"].as_str() {
                    return Ok(out.to_string());
                }
                if let Some(s) = first.as_str() {
                    return Ok(s.to_string());
                }
            }
        }
        Err("ICiba: empty result".to_string())
    }

    // ========== Transmart (腾讯通天塔) - 免Key ==========
    async fn translate_transmart(
        &self,
        text: &str,
        from: &str,
        to: &str,
    ) -> Result<String, String> {
        let body = serde_json::json!({
            "header": {
                "fn": "auto_translation_block",
                "client_key": "browser-chrome-110.0.0-Mac OS-df4bd4c5-a65d-44b2-a40f-42f34f3535f2-1677486696487"
            },
            "type": "plain",
            "model_category": "normal",
            "source": {
                "lang": from,
                "text_block": text
            },
            "target": {
                "lang": to
            }
        });

        let resp = self
            .client
            .post("https://transmart.qq.com/api/imt")
            .header("Referer", "https://yi.qq.com/zh-CN/index")
            .header(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            )
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("Transmart HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("Transmart JSON error: {e}"))?;

        json["auto_translation"]
            .as_str()
            .map(|s| s.to_string())
            .ok_or_else(|| "Transmart: empty result".to_string())
    }

    // ========== Yandex - 免Key ==========
    async fn translate_yandex(&self, text: &str, from: &str, to: &str) -> Result<String, String> {
        let ucid = uuid::Uuid::new_v4();
        let lang = if from == "auto" || from.is_empty() {
            to.to_string()
        } else {
            format!("{from}-{to}")
        };
        let url = format!(
            "https://translate.yandex.net/api/v1/tr.json/translate?ucid={ucid}&srv=android&format=text"
        );

        let resp = self
            .client
            .post(&url)
            .header("User-Agent", "ru.yandex.translate/3.20.2024")
            .form(&[("text", text), ("lang", &lang)])
            .send()
            .await
            .map_err(|e| format!("Yandex HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("Yandex JSON error: {e}"))?;

        json["text"]
            .as_array()
            .and_then(|arr| arr.first())
            .and_then(|v| v.as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| "Yandex: empty result".to_string())
    }

    // ========== Baidu 翻译 - 需AppID+AppKey ==========
    async fn translate_baidu(&self, text: &str, from: &str, to: &str) -> Result<String, String> {
        if self.config.baidu_app_id.is_empty() || self.config.baidu_app_key.is_empty() {
            return Err("百度翻译未配置AppID/AppKey".to_string());
        }
        let salt = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs()
            .to_string();
        let sign_raw = format!(
            "{}{}{}{}",
            self.config.baidu_app_id, text, salt, self.config.baidu_app_key
        );
        let mut hasher = Md5::new();
        hasher.update(sign_raw.as_bytes());
        let sign = format!("{:x}", hasher.finalize());
        let url = format!(
            "https://fanyi-api.baidu.com/api/trans/vip/translate?q={}&from={}&to={}&appid={}&salt={}&sign={}",
            urlencoding::encode(text),
            from,
            to,
            self.config.baidu_app_id,
            salt,
            sign
        );

        let resp = self
            .client
            .get(&url)
            .send()
            .await
            .map_err(|e| format!("Baidu HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("Baidu JSON error: {e}"))?;

        if let Some(err_code) = json["error_code"].as_str() {
            return Err(format!("Baidu {}: {}", err_code, json["error_msg"]));
        }
        json["trans_result"]
            .as_array()
            .and_then(|arr| arr.first())
            .and_then(|r| r["dst"].as_str())
            .map(|s| s.to_string())
            .ok_or_else(|| "Baidu: empty result".to_string())
    }

    // ========== BigModel 智谱GLM - 需API Key ==========
    async fn translate_bigmodel(&self, text: &str, from: &str, to: &str) -> Result<String, String> {
        if self.config.bigmodel_key.is_empty() {
            return Err("智谱BigModel未配置API Key".to_string());
        }
        let lang_map = |code: &str| -> &str {
            match code {
                "zh" | "zh-CN" | "zh-CHS" => "Simplified Chinese",
                "zh-TW" | "zh-CHT" => "Traditional Chinese",
                "en" => "English",
                "ja" => "Japanese",
                "ko" => "Korean",
                "fr" => "French",
                "es" => "Spanish",
                "ru" => "Russian",
                "de" => "German",
                "it" => "Italian",
                "tr" => "Turkish",
                "pt" => "Portuguese",
                "vi" => "Vietnamese",
                "id" => "Indonesian",
                "th" => "Thai",
                "ar" => "Arabic",
                "hi" => "Hindi",
                _ => "English",
            }
        };
        let src_lang = lang_map(from);
        let dst_lang = lang_map(to);
        let prompt = format!(
            "Translate the following text from {src_lang} to {dst_lang}. Only output the translation, nothing else.\n\nText: {text}"
        );
        let body = serde_json::json!({
            "model": "glm-4.7-flash",
            "messages": [{"role": "user", "content": prompt}],
            "max_tokens": 2000,
            "stream": false,
            "thinking": {"type": "disabled"}
        });

        let resp = self
            .client
            .post("https://open.bigmodel.cn/api/paas/v4/chat/completions")
            .header("Authorization", format!("Bearer {}", self.config.bigmodel_key))
            .json(&body)
            .send()
            .await
            .map_err(|e| format!("BigModel HTTP error: {e}"))?;

        let json: Value = resp
            .json()
            .await
            .map_err(|e| format!("BigModel JSON error: {e}"))?;

        if let Some(err) = json["error"].as_object() {
            return Err(format!("BigModel {}: {}", err["code"], err["message"]));
        }
        json["choices"]
            .as_array()
            .and_then(|arr| arr.first())
            .and_then(|c| c["message"]["content"].as_str())
            .map(|s| s.trim().to_string())
            .ok_or_else(|| "BigModel: empty result".to_string())
    }
}