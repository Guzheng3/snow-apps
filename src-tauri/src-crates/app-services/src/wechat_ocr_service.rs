//! WeChat OCR service — calls the WeChat OCR helper process (C#) via command line.
//! The helper must be compiled separately from the wechat-ocr-helper/ directory.
//! Requires WeChat installed on the system (for the native OCR DLLs).

use std::path::PathBuf;
use std::time::Duration;

use serde::Deserialize;
use serde::Serialize;
use tokio::process::Command;
use tokio::time::timeout;

/// Result from the WeChat OCR helper matching the OcrDetectResult shape.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeChatOcrOutput {
    pub text_blocks: Vec<WeChatTextBlock>,
    pub scale_factor: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeChatTextBlock {
    pub box_points: Vec<WeChatPoint>,
    pub text: String,
    pub text_score: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeChatPoint {
    pub x: f32,
    pub y: f32,
}

pub struct WeChatOcrService {
    /// Path to the compiled C# helper executable
    helper_path: Option<PathBuf>,
    /// Optional custom WeChat installation directory
    wechat_dir: Option<PathBuf>,
    /// Whether the service is initialized
    initialized: bool,
}

impl WeChatOcrService {
    pub fn new() -> Self {
        Self {
            helper_path: None,
            wechat_dir: None,
            initialized: false,
        }
    }

    /// Initialize the service with the helper executable path and optional WeChat directory.
    pub fn init(&mut self, helper_path: PathBuf, wechat_dir: Option<PathBuf>) -> Result<(), String> {
        if !helper_path.exists() {
            return Err(format!(
                "[WeChatOcrService] helper not found at: {}",
                helper_path.display()
            ));
        }

        self.helper_path = Some(helper_path);
        self.wechat_dir = wechat_dir;
        self.initialized = true;

        log::info!(
            "[WeChatOcrService] initialized with helper: {:?}, wechat_dir: {:?}",
            self.helper_path,
            self.wechat_dir,
        );

        Ok(())
    }

    /// Run OCR on the given image data.
    /// The image is written to a temp file and passed to the C# helper.
    pub async fn detect(&self, image_data: &[u8]) -> Result<WeChatOcrOutput, String> {
        if !self.initialized {
            return Err("[WeChatOcrService] not initialized".to_string());
        }

        let helper_path = self
            .helper_path
            .as_ref()
            .ok_or("[WeChatOcrService] helper_path not set")?;

        // Write image to temp file
        let temp_dir = std::env::temp_dir().join("snowshot_wechat_ocr");
        std::fs::create_dir_all(&temp_dir).map_err(|e| {
            format!("[WeChatOcrService] failed to create temp dir: {}", e)
        })?;

        let temp_file = temp_dir.join(format!("ocr_{}.png", std::process::id()));
        std::fs::write(&temp_file, image_data).map_err(|e| {
            format!("[WeChatOcrService] failed to write temp image: {}", e)
        })?;

        let result = self.run_helper(helper_path, &temp_file).await;

        // Clean up temp file
        let _ = std::fs::remove_file(&temp_file);

        result
    }

    async fn run_helper(
        &self,
        helper_path: &PathBuf,
        image_path: &PathBuf,
    ) -> Result<WeChatOcrOutput, String> {
        let mut cmd = Command::new(helper_path);
        cmd.arg(image_path);

        if let Some(ref wechat_dir) = self.wechat_dir {
            cmd.arg("--wechat-dir");
            cmd.arg(wechat_dir);
        }

        cmd.stdout(std::process::Stdio::piped());
        cmd.stderr(std::process::Stdio::piped());

        let mut child = cmd
            .spawn()
            .map_err(|e| format!("[WeChatOcrService] failed to start helper: {}", e))?;

        let exit_status = timeout(Duration::from_secs(30), child.wait())
            .await
            .map_err(|_| {
                "[WeChatOcrService] helper timed out after 30s".to_string()
            })?
            .map_err(|e| format!("[WeChatOcrService] helper process error: {}", e))?;

        if !exit_status.success() {
            let stderr_output = child
                .stderr
                .take()
                .map(|mut s| {
                    use std::io::Read;
                    let mut buf = vec![];
                    s.read_to_end(&mut buf).ok();
                    String::from_utf8_lossy(&buf).to_string()
                })
                .unwrap_or_default();

            // Kill the process if it's still running
            let _ = child.kill().await;

            return Err(format!(
                "[WeChatOcrService] helper exited with code {}: {}",
                exit_status.code().unwrap_or(-1),
                stderr_output.trim(),
            ));
        }

        let stdout = child.stdout.as_mut().ok_or(
            "[WeChatOcrService] failed to capture stdout".to_string(),
        )?;
        let mut output = String::new();
        use std::io::Read;
        stdout
            .read_to_string(&mut output)
            .map_err(|e| format!("[WeChatOcrService] failed to read stdout: {}", e))?;

        if output.trim().is_empty() {
            return Err("[WeChatOcrService] helper returned empty output".to_string());
        }

        serde_json::from_str::<WeChatOcrOutput>(&output).map_err(|e| {
            format!(
                "[WeChatOcrService] failed to parse helper output: {} — raw: {}",
                e,
                output.chars().take(200).collect::<String>()
            )
        })
    }

    /// Release the service (currently a no-op; the helper is stateless).
    pub fn release(&mut self) {
        self.initialized = false;
        self.helper_path = None;
        self.wechat_dir = None;
        log::info!("[WeChatOcrService] released");
    }
}