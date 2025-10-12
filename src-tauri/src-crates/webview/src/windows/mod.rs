use std::sync::mpsc::channel;
use webview2_com::Microsoft::Web::WebView2::Win32::{
    COREWEBVIEW2_SHARED_BUFFER_ACCESS_READ_WRITE, ICoreWebView2_17, ICoreWebView2Environment12,
};
use windows_core::Interface;

pub async fn create_shared_buffer(
    webview: tauri::Webview,
    data: &[u8],
    extra_data: &[u8],
) -> Result<(), String> {
    // windows 可以使用 SharedBuffer 加快数据传输
    let (transfer_result_sender, transfer_result_receiver) = channel::<Result<(), String>>();
    // 使用 unsafe 将引用转换为 'static 生命周期，因为保证数据在 with_webview 执行期间有效
    let data_static: &'static [u8] = unsafe { std::mem::transmute(data) };
    let extra_data_static: &'static [u8] = unsafe { std::mem::transmute(extra_data) };

    let sender = transfer_result_sender.clone();
    match webview.with_webview(move |webview| {
        let environment = webview.environment();

        let core_webview = match unsafe { webview.controller().CoreWebView2() } {
            Ok(core_webview) => core_webview,
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to get core webview: {:?}",
                        e
                    )))
                    .unwrap();
                return;
            }
        };

        let enviroment_12 = match environment.cast::<ICoreWebView2Environment12>() {
            Ok(environment) => environment,
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to create shared buffer: {:?}",
                        e
                    )))
                    .unwrap();
                return;
            }
        };

        let data_len = data_static.len() + extra_data_static.len();
        let shared_buffer = match unsafe { enviroment_12.CreateSharedBuffer(data_len as u64) } {
            Ok(sharedbuffer) => sharedbuffer,
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to create shared buffer: {:?}",
                        e
                    )))
                    .unwrap();
                return;
            }
        };

        let mut shared_buffer_ptr: *mut u8 = 0 as *mut u8;
        match unsafe { shared_buffer.Buffer(&mut shared_buffer_ptr) } {
            Ok(_) => (),
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to buffer shared buffer: {:?}",
                        e
                    )))
                    .unwrap();
                return;
            }
        };

        let webview_17 = match core_webview.cast::<ICoreWebView2_17>() {
            Ok(environment) => environment,
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to cast to ICoreWebView2_17: {:?}",
                        e
                    )))
                    .unwrap();
                return;
            }
        };

        // 将数据拷贝到 shared_buffer
        unsafe {
            std::ptr::copy_nonoverlapping(
                data_static.as_ptr(),
                shared_buffer_ptr,
                data_static.len(),
            );
            std::ptr::copy_nonoverlapping(
                extra_data_static.as_ptr(),
                shared_buffer_ptr.add(data_static.len()),
                extra_data_static.len(),
            );
        }

        match unsafe {
            webview_17.PostSharedBufferToScript(
                &shared_buffer,
                COREWEBVIEW2_SHARED_BUFFER_ACCESS_READ_WRITE,
                windows::core::PCWSTR::default(),
            )
        } {
            Ok(_) => {
                sender.send(Ok(())).unwrap();
            }
            Err(e) => {
                sender
                    .send(Err(format!(
                        "[create_shared_buffer] Failed to post shared buffer to script: {:?}",
                        e
                    )))
                    .unwrap();
            }
        };
    }) {
        Ok(_) => {}
        Err(e) => {
            transfer_result_sender
                .send(Err(format!(
                    "[create_shared_buffer] Failed to create shared buffer: {:?}",
                    e
                )))
                .unwrap();
        }
    }

    let result = match transfer_result_receiver.recv() {
        Ok(result) => result,
        Err(_) => {
            return Err(format!(
                "[create_shared_buffer] Failed to receive transfer result",
            ));
        }
    };

    result?;

    Ok(())
}
