pub mod core;
pub mod file;
pub mod listen_key;
pub mod ocr;
pub mod screenshot;
pub mod scroll_screenshot;
pub mod video_record;

use std::sync::Arc;
use tauri::Emitter;
use tokio::sync::Mutex;

use tauri::Manager;

use snow_shot_app_os::ui_automation::UIElements;
use snow_shot_app_scroll_screenshot_service::scroll_screenshot_capture_service;
use snow_shot_app_scroll_screenshot_service::scroll_screenshot_image_service;
use snow_shot_app_scroll_screenshot_service::scroll_screenshot_service;
use snow_shot_app_services::file_cache_service;
use snow_shot_app_services::free_drag_window_service;
use snow_shot_app_services::listen_key_service;
use snow_shot_app_services::ocr_service::OcrService;
use snow_shot_app_services::video_record_service;
use snow_shot_app_shared::EnigoManager;

#[cfg(feature = "dhat-heap")]
pub static PROFILER: std::sync::LazyLock<Mutex<Option<dhat::Profiler>>> =
    std::sync::LazyLock::new(|| Mutex::new(None));

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let ocr_instance = Mutex::new(OcrService::new());
    let video_record_service = Mutex::new(video_record_service::VideoRecordService::new());

    let enigo_instance = Mutex::new(EnigoManager::new());

    let ui_elements = Mutex::new(UIElements::new());

    let scroll_screenshot_service =
        Mutex::new(scroll_screenshot_service::ScrollScreenshotService::new());
    let scroll_screenshot_image_service =
        Mutex::new(scroll_screenshot_image_service::ScrollScreenshotImageService::new());
    let scroll_screenshot_capture_service =
        Mutex::new(scroll_screenshot_capture_service::ScrollScreenshotCaptureService::new());

    let free_drag_window_service =
        Mutex::new(free_drag_window_service::FreeDragWindowService::new());

    let listen_key_service = Mutex::new(listen_key_service::ListenKeyService::new());

    let file_cache_service = Arc::new(file_cache_service::FileCacheService::new());

    tauri::Builder::default()
        .plugin(
            tauri_plugin_window_state::Builder::new()
                .with_state_flags(
                    tauri_plugin_window_state::StateFlags::SIZE
                        | tauri_plugin_window_state::StateFlags::POSITION,
                )
                .with_filter(|label| label == "main")
                .build(),
        )
        .plugin(tauri_plugin_os::init())
        .plugin(tauri_plugin_single_instance::init(|app, _, _| {
            let app_window = app.get_webview_window("main").expect("no main window");
            app_window.unminimize().unwrap();
            app_window.set_focus().unwrap();
            app_window.show().unwrap();
        }))
        .plugin(tauri_plugin_macos_permissions::init())
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_clipboard::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .plugin(tauri_plugin_store::Builder::new().build())
        .plugin(tauri_plugin_http::init())
        .plugin(tauri_plugin_autostart::init(
            tauri_plugin_autostart::MacosLauncher::LaunchAgent,
            Some(vec!["--auto_start"]),
        ))
        .plugin(tauri_plugin_clipboard::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_notification::init())
        .plugin(tauri_plugin_process::init())
        .setup(|app| {
            use tauri_plugin_log::{Target, TargetKind};

            // let current_date = chrono::Local::now().format("%Y-%m-%d").to_string();

            // log 文件可能因为某些异常情况不断输出，造成日志文件过大
            // 先在 release 下屏蔽日志输出
            // 注意不要移除 log 插件的初始化,避免前端调用 log 时保存再次报错,持续循环报错
            let log_targets: Vec<Target> = if cfg!(debug_assertions) {
                vec![
                    Target::new(TargetKind::Stdout),
                    Target::new(TargetKind::LogDir { file_name: None }),
                    Target::new(TargetKind::Webview),
                ]
            } else {
                vec![Target::new(TargetKind::Stdout)]
            };

            app.handle().plugin(
                tauri_plugin_log::Builder::default()
                    .rotation_strategy(tauri_plugin_log::RotationStrategy::KeepAll)
                    .timezone_strategy(tauri_plugin_log::TimezoneStrategy::UseLocal)
                    .targets(log_targets)
                    .level(log::LevelFilter::Info)
                    .build(),
            )?;

            let main_window = app
                .get_webview_window("main")
                .expect("[lib::setup] no main window");

            #[cfg(target_os = "windows")]
            {
                match main_window.set_decorations(false) {
                    Ok(_) => (),
                    Err(_) => {
                        log::error!("[init_main_window] Failed to set decorations");
                    }
                }
            }

            #[cfg(target_os = "macos")]
            {
                // macOS 下不在 dock 显示
                app.set_activation_policy(tauri::ActivationPolicy::Accessory);
            }

            // 监听窗口关闭事件，拦截关闭按钮
            let window_clone = main_window.clone();
            main_window.on_window_event(move |event| {
                if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                    api.prevent_close();

                    // 隐藏窗口而不是关闭
                    if let Err(e) = window_clone.hide() {
                        log::error!("[macos] hide window error: {:?}", e);
                    }
                    window_clone.emit("on-hide-main-window", ()).unwrap();
                }
            });

            // 如果是调试模式，则显示窗口
            #[cfg(debug_assertions)]
            {
                main_window.show().unwrap();
            }

            Ok(())
        })
        .manage(ui_elements)
        .manage(ocr_instance)
        .manage(enigo_instance)
        .manage(scroll_screenshot_service)
        .manage(scroll_screenshot_image_service)
        .manage(scroll_screenshot_capture_service)
        .manage(video_record_service)
        .manage(free_drag_window_service)
        .manage(listen_key_service)
        .manage(file_cache_service)
        .invoke_handler(tauri::generate_handler![
            screenshot::capture_current_monitor,
            screenshot::capture_all_monitors,
            screenshot::capture_focused_window,
            screenshot::get_window_elements,
            screenshot::init_ui_elements,
            screenshot::get_element_from_position,
            screenshot::init_ui_elements_cache,
            screenshot::get_mouse_position,
            screenshot::create_draw_window,
            screenshot::switch_always_on_top,
            screenshot::set_draw_window_style,
            file::save_file,
            file::write_file,
            file::copy_file,
            file::remove_file,
            file::create_dir,
            file::remove_dir,
            file::get_app_config_dir,
            file::get_app_config_base_dir,
            file::create_local_config_dir,
            ocr::ocr_detect,
            ocr::ocr_init,
            ocr::ocr_release,
            core::exit_app,
            core::start_free_drag,
            core::close_window_after_delay,
            core::get_selected_text,
            core::set_enable_proxy,
            core::scroll_through,
            core::click_through,
            core::create_fixed_content_window,
            core::read_image_from_clipboard,
            core::create_full_screen_draw_window,
            core::get_current_monitor_info,
            core::get_monitors_bounding_box,
            core::send_new_version_notification,
            core::create_video_record_window,
            core::set_current_window_always_on_top,
            core::auto_start_enable,
            core::auto_start_disable,
            core::restart_with_admin,
            core::write_bitmap_image_to_clipboard,
            scroll_screenshot::scroll_screenshot_get_image_data,
            scroll_screenshot::scroll_screenshot_init,
            scroll_screenshot::scroll_screenshot_capture,
            scroll_screenshot::scroll_screenshot_handle_image,
            scroll_screenshot::scroll_screenshot_save_to_file,
            scroll_screenshot::scroll_screenshot_save_to_clipboard,
            scroll_screenshot::scroll_screenshot_get_size,
            scroll_screenshot::scroll_screenshot_clear,
            video_record::video_record_start,
            video_record::video_record_stop,
            video_record::video_record_pause,
            video_record::video_record_resume,
            video_record::video_record_kill,
            video_record::video_record_get_microphone_device_names,
            video_record::video_record_init,
            listen_key::listen_key_start,
            listen_key::listen_key_stop,
            listen_key::listen_key_stop_by_window_label,
            file::text_file_read,
            file::text_file_write,
            file::text_file_clear,
        ])
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { .. } = event {
                let window_label = window.label().to_owned();

                // 用 tokio 异步进程实现清除有异步所有权问题，通知前端清理，简单处理
                match window
                    .app_handle()
                    .emit("listen-key-service:stop", window_label)
                {
                    Ok(_) => (),
                    Err(e) => {
                        log::error!("[listen_key_service:stop] Failed to emit event: {}", e);
                    }
                }
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
