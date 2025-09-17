use enigo::{Axis, Mouse};
use serde::Serialize;
use std::{
    path::PathBuf,
    time::{SystemTime, UNIX_EPOCH},
};
use tauri::Emitter;
use tauri::Manager;
use tokio::{sync::Mutex, time::Duration};

use snow_shot_app_os::notification;
use snow_shot_app_services::free_drag_window_service::FreeDragWindowService;
use snow_shot_app_shared::{ElementRect, EnigoManager};
use snow_shot_app_utils::{get_target_monitor, monitor_info::MonitorRect};

pub async fn exit_app(window: tauri::Window, handle: tauri::AppHandle) {
    window.hide().unwrap();
    handle.exit(0);
}

pub async fn get_selected_text() -> String {
    let text = match get_selected_text::get_selected_text() {
        Ok(text) => text,
        Err(_) => {
            return String::new();
        }
    };
    text
}

pub async fn set_enable_proxy(enable: bool, host: String) -> Result<(), ()> {
    unsafe {
        if enable {
            std::env::set_var("NO_PROXY", "");
        } else {
            std::env::set_var("NO_PROXY", host);
        }
    }
    Ok(())
}

/// 鼠标滚轮穿透
pub async fn scroll_through(
    window: tauri::Window,
    enigo_manager: tauri::State<'_, Mutex<EnigoManager>>,
    length: i32,
) -> Result<(), String> {
    let mut enigo = enigo_manager.lock().await;
    let enigo = enigo.get_enigo()?;

    let result = window.set_ignore_cursor_events(true);
    if result.is_err() {
        return Err(String::from(
            "[scroll_through] Failed to set ignore cursor events",
        ));
    }

    tokio::time::sleep(Duration::from_millis(1)).await;

    {
        match enigo.scroll(length, Axis::Vertical) {
            Ok(_) => (),
            Err(e) => {
                log::error!("[scroll_through] scroll error: {}", e);
            }
        }
    }

    tokio::time::sleep(Duration::from_millis(128)).await;
    let _ = window.set_ignore_cursor_events(false);

    Ok(())
}

/// 鼠标滚轮穿透
pub async fn click_through(window: tauri::Window) -> Result<(), ()> {
    let result = window.set_ignore_cursor_events(true);
    if result.is_err() {
        return Ok(());
    }

    tokio::time::sleep(Duration::from_millis(128)).await;
    match window.set_ignore_cursor_events(false) {
        Ok(_) => (),
        Err(_) => (),
    }

    Ok(())
}

/// 创建内容固定到屏幕的窗口
pub async fn create_fixed_content_window(
    app: tauri::AppHandle,
    scroll_screenshot: bool,
) -> Result<(), String> {
    let (_, _, monitor) = get_target_monitor()?;

    let monitor_x = monitor.x().unwrap() as f64;
    let monitor_y = monitor.y().unwrap() as f64;

    let window_x;
    let window_y;
    #[cfg(target_os = "macos")]
    {
        window_x = monitor_x;
        window_y = monitor_y;
    }
    #[cfg(not(target_os = "macos"))]
    {
        let monitor_scale_factor = monitor.scale_factor().unwrap() as f64;
        window_x = monitor_x / monitor_scale_factor;
        window_y = monitor_y / monitor_scale_factor;
    }

    let window = tauri::WebviewWindowBuilder::new(
        &app,
        format!(
            "fixed-content-{}",
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_secs()
        ),
        tauri::WebviewUrl::App(PathBuf::from(format!(
            "/fixedContent?scroll_screenshot={}",
            scroll_screenshot
        ))),
    )
    .always_on_top(true)
    .resizable(false)
    .maximizable(false)
    .minimizable(false)
    .fullscreen(false)
    .title("Snow Shot - Fixed Content")
    .position(window_x, window_y)
    .decorations(false)
    .shadow(false)
    .transparent(true)
    .skip_taskbar(true)
    .resizable(false)
    .inner_size(1.0, 1.0)
    .build()
    .unwrap();

    window.hide().unwrap();
    window.center().unwrap();

    Ok(())
}

/// 创建全屏绘制窗口
pub async fn create_full_screen_draw_window(app: tauri::AppHandle) -> Result<(), String> {
    let window_label = "full-screen-draw";

    // 首先先查询是否存在窗口
    let window = app.get_webview_window(window_label);

    if let Some(window) = window {
        // 发送改变鼠标穿透的消息
        window
            .emit("full-screen-draw-change-mouse-through", ())
            .unwrap();

        return Ok(());
    }

    let (_, _, monitor) = get_target_monitor()?;

    let monitor_x = monitor.x().unwrap() as f64;
    let monitor_y = monitor.y().unwrap() as f64;
    let monitor_width = monitor.width().unwrap() as f64;
    let monitor_height = monitor.height().unwrap() as f64;

    let main_window = tauri::WebviewWindowBuilder::new(
        &app,
        window_label,
        tauri::WebviewUrl::App(PathBuf::from(format!("/fullScreenDraw"))),
    )
    .always_on_top(true)
    .resizable(false)
    .maximizable(false)
    .minimizable(false)
    .title("Snow Shot - Full Screen Draw")
    .position(0.0, 0.0)
    .inner_size(1.0, 1.0)
    .decorations(false)
    .shadow(false)
    .transparent(true)
    .skip_taskbar(true)
    .resizable(false)
    .build()
    .unwrap();

    tauri::WebviewWindowBuilder::new(
        &app,
        format!("{}_switch_mouse_through", window_label),
        tauri::WebviewUrl::App(PathBuf::from(format!(
            "/fullScreenDraw/switchMouseThrough?monitor_x={}&monitor_y={}&monitor_width={}&monitor_height={}",
            monitor_x,
            monitor_y,
            monitor_width,
            monitor_height
        ))),
    )
    .always_on_top(true)
    .resizable(false)
    .maximizable(false)
    .minimizable(false)
    .title("Snow Shot - Full Screen Draw - Switch Mouse Through")
    .position(0.0, 0.0)
    .inner_size(1.0, 1.0)
    .decorations(false)
    .shadow(false)
    .transparent(true)
    .skip_taskbar(true)
    .resizable(false)
    .build()
    .unwrap();

    #[cfg(target_os = "macos")]
    {
        main_window
            .set_position(tauri::LogicalPosition::new(monitor_x, monitor_y))
            .unwrap();
        main_window
            .set_size(tauri::LogicalSize::new(monitor_width, monitor_height))
            .unwrap();
    }

    #[cfg(not(target_os = "macos"))]
    {
        main_window
            .set_position(tauri::PhysicalPosition::new(monitor_x, monitor_y))
            .unwrap();
        main_window
            .set_size(tauri::PhysicalSize::new(monitor_width, monitor_height))
            .unwrap();
    }

    Ok(())
}

#[derive(Serialize, Clone, Copy)]
pub struct MonitorInfo {
    mouse_x: i32,
    mouse_y: i32,
    monitor_x: i32,
    monitor_y: i32,
    monitor_width: u32,
    monitor_height: u32,
    monitor_scale_factor: f32,
}

pub async fn get_current_monitor_info() -> Result<MonitorInfo, String> {
    #[cfg(target_os = "macos")]
    let (mut mouse_x, mut mouse_y, monitor) = get_target_monitor()?;
    #[cfg(not(target_os = "macos"))]
    let (mouse_x, mouse_y, monitor) = get_target_monitor()?;

    let monitor_x = monitor.x().unwrap();
    let monitor_y = monitor.y().unwrap();

    #[cfg(target_os = "macos")]
    let mut monitor_width = monitor.width().unwrap();
    #[cfg(not(target_os = "macos"))]
    let monitor_width = monitor.width().unwrap();

    #[cfg(target_os = "macos")]
    let mut monitor_height = monitor.height().unwrap();
    #[cfg(not(target_os = "macos"))]
    let monitor_height = monitor.height().unwrap();

    let monitor_scale_factor = monitor.scale_factor().unwrap();

    // macOS 下，屏幕宽高是逻辑像素，这里统一转换为物理像素
    #[cfg(target_os = "macos")]
    {
        monitor_width = (monitor_width as f32 * monitor_scale_factor) as u32;
        monitor_height = (monitor_height as f32 * monitor_scale_factor) as u32;
        // 把鼠标坐标转换为物理像素
        mouse_x = (mouse_x as f32 * monitor_scale_factor) as i32;
        mouse_y = (mouse_y as f32 * monitor_scale_factor) as i32;
    }

    let monitor_info = MonitorInfo {
        mouse_x: mouse_x - monitor_x,
        mouse_y: mouse_y - monitor_y,
        monitor_x: monitor_x,
        monitor_y: monitor_y,
        monitor_width: monitor_width,
        monitor_height: monitor_height,
        monitor_scale_factor: monitor_scale_factor,
    };
    Ok(monitor_info)
}

#[derive(Serialize, Clone)]
pub struct MonitorsBoundingBox {
    rect: ElementRect,
    monitor_rect_list: Vec<MonitorRect>,
}

pub async fn get_monitors_bounding_box(
    app: &tauri::AppHandle,
    region: Option<ElementRect>,
) -> Result<MonitorsBoundingBox, String> {
    let monitors = snow_shot_app_utils::get_capture_monitor_list(app, region)?;

    let monitors_bounding_box = monitors.get_monitors_bounding_box();

    Ok(MonitorsBoundingBox {
        rect: monitors_bounding_box,
        monitor_rect_list: monitors.monitor_rect_list(),
    })
}

pub async fn send_new_version_notification(title: String, body: String) {
    notification::send_new_version_notification(title, body);
}

#[derive(Serialize, Clone, Copy)]
struct VideoRecordWindowInfo {
    select_rect_min_x: i32,
    select_rect_min_y: i32,
    select_rect_max_x: i32,
    select_rect_max_y: i32,
}

/// 创建屏幕录制窗口
pub async fn create_video_record_window(
    app: tauri::AppHandle,
    select_rect_min_x: i32,
    select_rect_min_y: i32,
    select_rect_max_x: i32,
    select_rect_max_y: i32,
) {
    let window_label = "video-recording";

    let window = app.get_webview_window(window_label);

    if let Some(window) = window {
        window
            .emit(
                "reload-video-record",
                VideoRecordWindowInfo {
                    select_rect_min_x,
                    select_rect_min_y,
                    select_rect_max_x,
                    select_rect_max_y,
                },
            )
            .unwrap();

        return;
    }

    tauri::WebviewWindowBuilder::new(
        &app,
        window_label,
        tauri::WebviewUrl::App(PathBuf::from(format!(
            "/videoRecord?select_rect_min_x={}&select_rect_min_y={}&select_rect_max_x={}&select_rect_max_y={}",
            select_rect_min_x,
            select_rect_min_y,
            select_rect_max_x,
            select_rect_max_y
        ))),
    )
    .always_on_top(true)
    .resizable(false)
    .maximizable(false)
    .minimizable(false)
    .title("Snow Shot - Video Record")
    .position(0.0, 0.0)
    .inner_size(1.0, 1.0)
    .decorations(false)
    .shadow(false)
    .transparent(true)
    .skip_taskbar(true)
    .resizable(false)
    .visible(false)
    .build()
    .unwrap();

    let window_label = "video-recording-toolbar";

    let window = app.get_webview_window(window_label);

    if let Some(window) = window {
        window.destroy().unwrap();
    }

    tauri::WebviewWindowBuilder::new(
        &app,
        window_label,
        tauri::WebviewUrl::App(PathBuf::from(format!(
            "/videoRecord/toolbar?select_rect_min_x={}&select_rect_min_y={}&select_rect_max_x={}&select_rect_max_y={}",
            select_rect_min_x,
            select_rect_min_y,
            select_rect_max_x,
            select_rect_max_y
        ))),
    )
    .always_on_top(true)
    .resizable(false)
    .maximizable(false)
    .minimizable(false)
    .title("Snow Shot - Video Record - Toolbar")
    .position(0.0, 0.0)
    .inner_size(1.0, 1.0)
    .decorations(false)
    .shadow(false)
    .transparent(true)
    .skip_taskbar(true)
    .resizable(false)
    .visible(false)
    .build()
    .unwrap();
}

pub async fn start_free_drag(
    window: tauri::Window,
    free_drag_window_service: tauri::State<'_, Mutex<FreeDragWindowService>>,
) -> Result<(), String> {
    let mut free_drag_window_service = free_drag_window_service.lock().await;

    free_drag_window_service.start_drag(window)?;

    Ok(())
}

pub async fn set_current_window_always_on_top(
    #[allow(unused_variables)] window: tauri::WebviewWindow,
    #[allow(unused_variables)] allow_input_method_overlay: bool,
) -> Result<(), String> {
    #[cfg(target_os = "macos")]
    {
        let window_ns = window.ns_window();

        let window_ns = match window_ns {
            Ok(window_ns) => window_ns as usize,
            Err(_) => {
                return Err(String::from(
                    "[set_current_window_always_on_top] Failed to get NSWindow",
                ));
            }
        };

        match window.run_on_main_thread(move || unsafe {
            use objc2::runtime::AnyObject;
            use objc2_app_kit::NSWindowCollectionBehavior;
            use std::ffi::c_void;

            let window_ns = window_ns as *mut c_void;

            if window_ns.is_null() {
                log::error!("[set_current_window_always_on_top] NSWindow is null");
                return;
            }

            let window_ns = window_ns as *mut AnyObject;

            // level 为 20 不遮挡输入法
            if allow_input_method_overlay {
                let _: () = objc2::msg_send![window_ns, setLevel: 20];
            } else {
                let _: () =
                    objc2::msg_send![window_ns, setLevel: objc2_app_kit::NSStatusWindowLevel + 1];
            }

            let _: () = objc2::msg_send![
                window_ns,
                setCollectionBehavior: NSWindowCollectionBehavior::CanJoinAllSpaces
                | NSWindowCollectionBehavior::Stationary
                | NSWindowCollectionBehavior::FullScreenAuxiliary,
            ];
        }) {
            Ok(_) => (),
            Err(_) => {
                return Err(String::from(
                    "[set_current_window_always_on_top] Failed to run on main thread",
                ));
            }
        }

        match window.set_focus() {
            Ok(_) => (),
            Err(_) => {
                log::warn!("[set_current_window_always_on_top] Failed to set focus");
            }
        }
    }

    Ok(())
}

pub async fn close_window_after_delay(window: tauri::Window, delay: u64) {
    // 用另一个进程运行，tauri 执行完命令后会发送消息给原窗口，一定时间后窗口可能已经提前销毁了
    // 发送消息给已经销毁的窗口会报错
    tokio::spawn(async move {
        tokio::time::sleep(Duration::from_millis(delay)).await;
        match window.close() {
            Ok(_) => (),
            Err(_) => log::info!("[close_window_after_delay] The window has been released"),
        }
    });
}

pub async fn create_admin_auto_start_task() -> Result<(), String> {
    snow_shot_app_os::utils::create_admin_auto_start_task()
}

pub async fn delete_admin_auto_start_task() -> Result<(), String> {
    snow_shot_app_os::utils::delete_admin_auto_start_task()
}

pub async fn restart_with_admin() -> Result<(), String> {
    snow_shot_app_os::utils::restart_with_admin()
}

pub async fn is_admin() -> Result<bool, String> {
    Ok(snow_shot_app_os::utils::is_admin())
}

pub async fn write_bitmap_image_to_clipboard(
    request: tauri::ipc::Request<'_>,
) -> Result<(), String> {
    let image_data = match request.body() {
        tauri::ipc::InvokeBody::Raw(data) => data,
        _ => {
            return Err(String::from(
                "[write_bitmap_image_to_clipboard] Invalid request body",
            ));
        }
    };

    #[cfg(not(target_os = "windows"))]
    {
        return Err(String::from(
            "[write_bitmap_image_to_clipboard] Not supported on this platform",
        ));
    }

    // 如果是 Windows 系统则尝试使用 DIB 格式写入到剪贴板
    // Windows 下使用 DIB 格式写入到剪贴板，比 BMP 文件格式更标准
    #[cfg(target_os = "windows")]
    {
        use clipboard_win::{Setter, formats, types::BITMAPINFOHEADER};
        use image::ImageDecoder;
        use rayon::prelude::*;
        use std::mem;

        let decoder = match image::codecs::png::PngDecoder::new(std::io::Cursor::new(image_data)) {
            Ok(decoder) => decoder,
            Err(_) => {
                return Err(String::from(
                    "[write_bitmap_image_to_clipboard] Failed to create PNG decoder",
                ));
            }
        };
        let (image_width, image_height) = decoder.dimensions();
        let image_width = image_width as usize;
        let image_height = image_height as usize;
        let image_total_bytes = decoder.total_bytes() as usize;
        let mut rgba_image = Vec::with_capacity(image_total_bytes);
        unsafe {
            rgba_image.set_len(image_total_bytes);
        }
        decoder.read_image(&mut rgba_image).unwrap();

        // 计算 DIB 数据大小：BITMAPINFOHEADER + 像素数据
        let header_size = mem::size_of::<BITMAPINFOHEADER>();
        let row_size = ((image_width * 3 + 3) / 4) * 4; // 4字节对齐
        let pixel_data_size = row_size * image_height;
        let total_size = header_size + pixel_data_size;

        let mut dib_data = Vec::with_capacity(total_size);
        unsafe {
            dib_data.set_len(total_size);
        }

        // 构建 BITMAPINFOHEADER
        let bmi_header = BITMAPINFOHEADER {
            biSize: header_size as u32,
            biWidth: image_width as i32,
            biHeight: image_height as i32, // 底部向上
            biPlanes: 1,
            biBitCount: 24,   // RGB
            biCompression: 0, // BI_RGB
            biSizeImage: pixel_data_size as u32,
            biXPelsPerMeter: 0,
            biYPelsPerMeter: 0,
            biClrUsed: 0,
            biClrImportant: 0,
        };

        // 将 header 写入为字节
        let header_bytes = unsafe {
            std::slice::from_raw_parts(
                &bmi_header as *const BITMAPINFOHEADER as *const u8,
                header_size,
            )
        };
        let dib_data_ptr = dib_data.as_mut_ptr();
        unsafe {
            std::ptr::copy_nonoverlapping(header_bytes.as_ptr(), dib_data_ptr, header_bytes.len());
        }

        let dib_data_ptr = unsafe { dib_data_ptr.offset(header_bytes.len() as isize) } as usize;
        let rgba_image_ptr = rgba_image.as_ptr() as usize;
        (0..image_height).into_par_iter().rev().for_each(|y| {
            let rgba_index_start = y * image_width * 4;
            let dib_index_start = (image_height - y - 1) * row_size;
            (0..image_width).into_par_iter().for_each(|x| {
                let dib_data_ptr = dib_data_ptr as *mut u8;
                let rgba_image_ptr = rgba_image_ptr as *const u8;

                let rgba_base_index = rgba_index_start + x * 4;
                let dib_base_index = dib_index_start + x * 3;
                unsafe {
                    dib_data_ptr
                        .add(dib_base_index)
                        .write(rgba_image_ptr.add(rgba_base_index + 2).read());
                    dib_data_ptr
                        .add(dib_base_index + 1)
                        .write(rgba_image_ptr.add(rgba_base_index + 1).read());
                    dib_data_ptr
                        .add(dib_base_index + 2)
                        .write(rgba_image_ptr.add(rgba_base_index).read());
                }
            });
        });

        let _clip = clipboard_win::Clipboard::new().unwrap();

        formats::RawData(formats::CF_DIB).write_clipboard(&dib_data).map_err(|e| {
            format!(
                "[write_bitmap_image_to_clipboard] Write CF_DIB to clipboard: {}",
                e
            )
        })?;

        drop(_clip);
    }

    Ok(())
}
