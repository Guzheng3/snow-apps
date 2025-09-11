use snow_shot_app_shared::{ElementRect, EnigoManager};
use snow_shot_tauri_commands_core::MonitorsBoundingBox;
use tauri::{Manager, command, ipc::Response};
use tauri_plugin_autostart::ManagerExt;
use tokio::sync::Mutex;

#[command]
pub async fn exit_app(window: tauri::Window, handle: tauri::AppHandle) {
    #[cfg(feature = "dhat-heap")]
    drop(crate::PROFILER.lock().await.take());

    snow_shot_tauri_commands_core::exit_app(window, handle).await;
}

#[command]
pub async fn get_selected_text() -> String {
    snow_shot_tauri_commands_core::get_selected_text().await
}

#[command]
pub async fn set_enable_proxy(enable: bool, host: String) -> Result<(), ()> {
    snow_shot_tauri_commands_core::set_enable_proxy(enable, host).await
}

/// 鼠标滚轮穿透
#[command]
pub async fn scroll_through(
    window: tauri::Window,
    enigo_manager: tauri::State<'_, Mutex<EnigoManager>>,
    length: i32,
) -> Result<(), String> {
    snow_shot_tauri_commands_core::scroll_through(window, enigo_manager, length).await
}

/// 鼠标滚轮穿透
#[command]
pub async fn click_through(window: tauri::Window) -> Result<(), ()> {
    snow_shot_tauri_commands_core::click_through(window).await
}

/// 创建内容固定到屏幕的窗口
#[command]
pub async fn create_fixed_content_window(
    app: tauri::AppHandle,
    scroll_screenshot: bool,
) -> Result<(), String> {
    snow_shot_tauri_commands_core::create_fixed_content_window(app, scroll_screenshot).await
}

#[command]
pub async fn read_image_from_clipboard(handle: tauri::AppHandle) -> Response {
    let clipboard = handle.state::<tauri_plugin_clipboard::Clipboard>();
    let image_data = match tauri_plugin_clipboard::Clipboard::read_image_binary(&clipboard) {
        Ok(image_data) => image_data,
        Err(_) => return Response::new(vec![]),
    };

    return Response::new(image_data);
}

/// 创建全屏绘制窗口
#[command]
pub async fn create_full_screen_draw_window(app: tauri::AppHandle) -> Result<(), String> {
    snow_shot_tauri_commands_core::create_full_screen_draw_window(app).await
}

#[command]
pub async fn get_current_monitor_info() -> Result<snow_shot_tauri_commands_core::MonitorInfo, String>
{
    snow_shot_tauri_commands_core::get_current_monitor_info().await
}

#[command]
pub async fn get_monitors_bounding_box(
    app: tauri::AppHandle,
    region: Option<ElementRect>,
) -> Result<MonitorsBoundingBox, String> {
    snow_shot_tauri_commands_core::get_monitors_bounding_box(&app, region).await
}

#[command]
pub async fn send_new_version_notification(title: String, body: String) {
    snow_shot_tauri_commands_core::send_new_version_notification(title, body).await;
}

/// 创建屏幕录制窗口
#[command]
pub async fn create_video_record_window(
    app: tauri::AppHandle,
    select_rect_min_x: i32,
    select_rect_min_y: i32,
    select_rect_max_x: i32,
    select_rect_max_y: i32,
) {
    snow_shot_tauri_commands_core::create_video_record_window(
        app,
        select_rect_min_x,
        select_rect_min_y,
        select_rect_max_x,
        select_rect_max_y,
    )
    .await
}

#[command]
pub async fn start_free_drag(
    window: tauri::Window,
    free_drag_window_service: tauri::State<
        '_,
        Mutex<snow_shot_app_services::free_drag_window_service::FreeDragWindowService>,
    >,
) -> Result<(), String> {
    snow_shot_tauri_commands_core::start_free_drag(window, free_drag_window_service).await
}

#[command]
pub async fn set_current_window_always_on_top(
    window: tauri::WebviewWindow,
    allow_input_method_overlay: bool,
) -> Result<(), String> {
    snow_shot_tauri_commands_core::set_current_window_always_on_top(
        window,
        allow_input_method_overlay,
    )
    .await
}

#[command]
pub async fn close_window_after_delay(window: tauri::Window, delay: u64) {
    snow_shot_tauri_commands_core::close_window_after_delay(window, delay).await
}

#[command]
pub async fn auto_start_enable(app: tauri::AppHandle) -> Result<(), String> {
    let autostart_manager = app.autolaunch();

    #[cfg(not(target_os = "windows"))]
    {
        return match autostart_manager.enable() {
            Ok(_) => Ok(()),
            Err(e) => Err(format!(
                "[auto_start_enable] Failed to enable autostart: {}",
                e,
            )),
        };
    }

    #[cfg(target_os = "windows")]
    {
        // 判断是否是管理员模式
        let is_admin = match snow_shot_tauri_commands_core::is_admin().await {
            Ok(is_admin) => is_admin,
            Err(_) => return Err(String::from("[auto_start_enable] Failed to check if admin")),
        };

        // 如果是管理员模式，则禁用普通的自启动方式，使用 Windows 的任务计划程序实现自启动
        if !is_admin {
            match autostart_manager.enable() {
                Ok(_) => (),
                Err(e) => {
                    return Err(format!(
                        "[auto_start_enable] Failed to enable autostart: {}",
                        e,
                    ));
                }
            }

            return Ok(());
        }

        // 禁用普通自启动方式
        match autostart_manager.disable() {
            Ok(_) => (),
            Err(e) => {
                // 如果 autostart_manager 不是设置了的状态，则可能报错
                // 所以不提前退出
                log::warn!("[auto_start_enable] Failed to disable autostart: {}", e);
            }
        }

        // 创建管理员自启动任务
        match snow_shot_tauri_commands_core::create_admin_auto_start_task().await {
            Ok(_) => (),
            Err(e) => {
                return Err(format!(
                    "[auto_start_enable] Failed to create admin auto start task: {}",
                    e,
                ));
            }
        }

        Ok(())
    }
}

#[command]
pub async fn auto_start_disable(app: tauri::AppHandle) -> Result<(), String> {
    let autostart_manager = app.autolaunch();

    // 先禁用普通自启动方式
    match autostart_manager.disable() {
        Ok(_) => (),
        Err(e) => {
            log::warn!("[auto_start_disable] Failed to disable autostart: {}", e);
        }
    }

    #[cfg(target_os = "windows")]
    {
        // 判断是否是管理员模式
        let is_admin = match snow_shot_tauri_commands_core::is_admin().await {
            Ok(is_admin) => is_admin,
            Err(_) => {
                return Err(String::from(
                    "[auto_start_disable] Failed to check if admin",
                ));
            }
        };

        if !is_admin {
            return Ok(());
        }

        // 删除管理员自启动任务
        match snow_shot_tauri_commands_core::delete_admin_auto_start_task().await {
            Ok(_) => (),
            Err(e) => {
                return Err(format!(
                    "[auto_start_disable] Failed to delete admin auto start task: {}",
                    e,
                ));
            }
        }

        Ok(())
    }
}

#[command]
pub async fn restart_with_admin() -> Result<(), String> {
    snow_shot_tauri_commands_core::restart_with_admin().await
}
