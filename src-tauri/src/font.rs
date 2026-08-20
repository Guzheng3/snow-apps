// 字体安装命令：用户级安装（无需管理员权限）
// - 复制 ttf 到 %LOCALAPPDATA%\Microsoft\Windows\Fonts
// - 写注册表 HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts
// - 通知系统字体更新（AddFontResourceW）
use std::path::PathBuf;
use tauri::{command, AppHandle, Manager};

pub const FONT_FILE_NAME: &str = "言念君子温其如玉(2).ttf";
pub const FONT_FAMILY_NAME: &str = "Aa言念君子 温其如玉";

/// 用户级字体目录（无需管理员）
fn user_fonts_dir() -> Option<PathBuf> {
    std::env::var_os("LOCALAPPDATA")
        .map(|p| PathBuf::from(p).join("Microsoft").join("Windows").join("Fonts"))
}

fn fonts_registry_key() -> &'static str {
    r"HKCU\Software\Microsoft\Windows NT\CurrentVersion\Fonts"
}

/// 检查字体是否已安装（用户级）
#[command]
pub fn is_font_installed(font_name: String) -> bool {
    // 1. 检查用户字体目录是否有对应 ttf
    if let Some(dir) = user_fonts_dir() {
        let target = dir.join(FONT_FILE_NAME);
        if target.exists() {
            return true;
        }
    }
    // 2. 检查注册表
    let value = format!("{} (TrueType)", font_name);
    let output = std::process::Command::new("reg")
        .args(["query", fonts_registry_key(), "/v", &value])
        .output();
    match output {
        Ok(o) => o.status.success(),
        Err(_) => false,
    }
}

/// 安装字体到用户级（无需管理员）
#[command]
pub fn install_font(app: AppHandle) -> Result<(), String> {
    // 1. 从打包资源读取 ttf
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("获取资源目录失败: {e}"))?;
    let src = resource_dir.join("fonts").join(FONT_FILE_NAME);
    if !src.exists() {
        return Err(format!("字体文件不存在: {}", src.display()));
    }

    // 2. 复制到用户字体目录
    let fonts_dir = user_fonts_dir().ok_or("无法获取用户字体目录")?;
    std::fs::create_dir_all(&fonts_dir).map_err(|e| e.to_string())?;
    let target = fonts_dir.join(FONT_FILE_NAME);
    if !target.exists() {
        std::fs::copy(&src, &target).map_err(|e| format!("复制字体失败: {e}"))?;
    }

    // 3. 写注册表（用户级）
    let value = format!("{} (TrueType)", FONT_FAMILY_NAME);
    let output = std::process::Command::new("reg")
        .args([
            "add",
            fonts_registry_key(),
            "/v",
            &value,
            "/t",
            "REG_SZ",
            "/d",
            FONT_FILE_NAME,
            "/f",
        ])
        .output()
        .map_err(|e| format!("写注册表失败: {e}"))?;
    if !output.status.success() {
        return Err(format!(
            "写注册表失败: {}",
            String::from_utf8_lossy(&output.stderr)
        ));
    }

    Ok(())
}
