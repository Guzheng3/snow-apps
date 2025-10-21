// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

#[cfg(feature = "dhat-heap")]
use app_lib::PROFILER;

#[cfg(feature = "dhat-heap")]
#[global_allocator]
static ALLOC: dhat::Alloc = dhat::Alloc;

#[cfg(feature = "dhat-heap")]
#[tokio::main]
async fn main() {
    #[cfg(feature = "dhat-heap")]
    PROFILER.lock().await.replace(dhat::Profiler::new_heap());

    snow_shot_lib::run();
}

#[cfg(target_os = "windows")]
#[global_allocator]
static GLOBAL: mimalloc::MiMalloc = mimalloc::MiMalloc;

#[cfg(target_os = "macos")]
#[global_allocator]
static GLOBAL: tikv_jemallocator::Jemalloc = tikv_jemallocator::Jemalloc;

#[cfg(not(feature = "dhat-heap"))]
fn main() {
    let default_panic = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        use std::backtrace::Backtrace;

        let backtrace = Backtrace::force_capture();
        log::error!("Panic: {info}\n{backtrace}");
        default_panic(info);
    }));

    snow_shot_lib::run();
}
