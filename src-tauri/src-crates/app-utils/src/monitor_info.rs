use image::{DynamicImage, GenericImageView, RgbImage, RgbaImage};
use rayon::iter::{
    IndexedParallelIterator, IntoParallelIterator, IntoParallelRefIterator, ParallelIterator,
};
use serde::{Deserialize, Serialize};
use snow_shot_app_shared::ElementRect;
use xcap::Monitor;

#[cfg(target_os = "windows")]
use crate::monitor_hdr_info::{self, MonitorHdrInfo};
#[cfg(target_os = "windows")]
use windows::Win32::Graphics::Gdi::HMONITOR;

#[derive(Debug)]
pub struct MonitorInfo {
    pub monitor: Monitor,
    pub rect: ElementRect,
    pub scale_factor: f32,
    #[cfg(target_os = "windows")]
    pub monitor_hdr_info: MonitorHdrInfo,
}

#[derive(Debug, Clone, Copy)]
pub enum ColorFormat {
    Rgba8,
    Rgb8,
}

#[derive(Serialize, Clone)]
pub struct MonitorRect {
    pub rect: ElementRect,
    pub scale_factor: f32,
}

#[derive(Debug, Clone, Copy)]
pub struct CaptureOption {
    pub color_format: ColorFormat,
    pub correct_hdr_color_algorithm: CorrectHdrColorAlgorithm,
}

impl MonitorInfo {
    pub fn new(monitor: &Monitor, monitor_hdr_info: MonitorHdrInfo) -> Self {
        let monitor_rect: ElementRect;
        let scale_factor: f32;

        #[cfg(target_os = "windows")]
        {
            let rect = monitor.get_dev_mode_w().unwrap();
            monitor_rect = ElementRect {
                min_x: unsafe { rect.Anonymous1.Anonymous2.dmPosition.x },
                min_y: unsafe { rect.Anonymous1.Anonymous2.dmPosition.y },
                max_x: unsafe { rect.Anonymous1.Anonymous2.dmPosition.x + rect.dmPelsWidth as i32 },
                max_y: unsafe {
                    rect.Anonymous1.Anonymous2.dmPosition.y + rect.dmPelsHeight as i32
                },
            };
            scale_factor = monitor.scale_factor().unwrap_or(0.0);

            MonitorInfo {
                monitor: monitor.clone(),
                rect: monitor_rect,
                scale_factor,
                monitor_hdr_info,
            }
        }

        #[cfg(target_os = "macos")]
        {
            let rect = monitor.bounds().unwrap();
            let monitor_scale_factor = monitor.scale_factor().unwrap_or(1.0) as f64;
            monitor_rect = ElementRect {
                min_x: (rect.origin.x * monitor_scale_factor) as i32,
                min_y: (rect.origin.y * monitor_scale_factor) as i32,
                max_x: ((rect.origin.x + rect.size.width) * monitor_scale_factor) as i32,
                max_y: ((rect.origin.y + rect.size.height) * monitor_scale_factor) as i32,
            };
            scale_factor = 0.0;

            MonitorInfo {
                monitor: monitor.clone(),
                rect: monitor_rect,
                scale_factor,
            }
        }
    }

    pub fn get_monitor_crop_region(&self, crop_region: ElementRect) -> ElementRect {
        let monitor_crop_region = self.rect.clip_rect(&ElementRect {
            min_x: crop_region.min_x,
            min_y: crop_region.min_y,
            max_x: crop_region.max_x,
            max_y: crop_region.max_y,
        });

        ElementRect {
            min_x: monitor_crop_region.min_x - self.rect.min_x,
            min_y: monitor_crop_region.min_y - self.rect.min_y,
            max_x: monitor_crop_region.max_x - self.rect.min_x,
            max_y: monitor_crop_region.max_y - self.rect.min_y,
        }
    }

    #[cfg(target_os = "windows")]
    pub fn get_monitor_handle(monitor: &Monitor) -> HMONITOR {
        use std::ffi::c_void;

        HMONITOR(monitor.id().unwrap() as *mut c_void)
    }

    /// 获取显示器设备名称
    #[cfg(target_os = "windows")]
    pub fn get_device_name(monitor: &Monitor) -> Result<String, String> {
        use widestring::U16CString;
        use windows::Win32::{
            Foundation::RECT,
            Graphics::Gdi::{GetMonitorInfoW, MONITORINFO, MONITORINFOEXW},
        };

        let mut monitor_info = MONITORINFOEXW {
            monitorInfo: MONITORINFO {
                cbSize: u32::try_from(std::mem::size_of::<MONITORINFOEXW>()).unwrap(),
                rcMonitor: RECT::default(),
                rcWork: RECT::default(),
                dwFlags: 0,
            },
            szDevice: [0; 32],
        };

        let result = unsafe {
            GetMonitorInfoW(
                Self::get_monitor_handle(monitor),
                std::ptr::addr_of_mut!(monitor_info).cast(),
            )
        };

        if !result.as_bool() {
            return Err(format!(
                "[MonitorInfo::get_device_name] Failed to get monitor info: {:?}",
                result
            ));
        }

        let device_name = match U16CString::from_vec_truncate(monitor_info.szDevice).to_string() {
            Ok(name) => name,
            Err(e) => {
                return Err(format!(
                    "[MonitorInfo::get_device_name] Failed to get device name: {:?}",
                    e
                ));
            }
        };

        Ok(device_name)
    }

    pub fn capture(
        &self,
        crop_area: Option<ElementRect>,
        #[allow(unused_variables)] exclude_window: Option<&tauri::Window>,
        capture_option: CaptureOption,
    ) -> Option<image::DynamicImage> {
        #[cfg(target_os = "macos")]
        {
            return super::capture_target_monitor(
                &self.monitor,
                crop_area,
                exclude_window,
                capture_option.color_format,
            );
        }

        #[cfg(target_os = "windows")]
        {
            use crate::windows_capture_image;

            if !(self.monitor_hdr_info.hdr_enabled
                && capture_option.correct_hdr_color_algorithm != CorrectHdrColorAlgorithm::None)
            {
                return super::capture_target_monitor(
                    &self.monitor,
                    crop_area,
                    exclude_window,
                    capture_option.color_format,
                );
            }

            return match windows_capture_image::capture_monitor_image(
                &self,
                crop_area,
                capture_option.color_format,
            ) {
                Ok(image) => Some(image),
                Err(e) => {
                    log::error!(
                        "[MonitorInfo::capture] Failed to capture monitor image: {:?}",
                        e
                    );
                    None
                }
            };
        }
    }
}

#[derive(Debug)]
pub struct MonitorList(Vec<MonitorInfo>);

#[derive(Serialize, Deserialize, Clone, Debug, Copy, PartialEq)]
pub enum CorrectHdrColorAlgorithm {
    None,
    Linear,
}

impl MonitorList {
    fn get_monitors(region: Option<ElementRect>) -> MonitorList {
        let monitors = Monitor::all().unwrap_or_default();

        let region = match region {
            Some(region) => region,
            None => ElementRect {
                min_x: i32::MIN,
                min_y: i32::MIN,
                max_x: i32::MAX,
                max_y: i32::MAX,
            },
        };

        #[cfg(target_os = "windows")]
        let monitor_hdr_info_map = monitor_hdr_info::get_all_monitors_sdr_info().unwrap();

        let monitor_info_list = monitors
            .iter()
            .map(|monitor| {
                #[cfg(target_os = "windows")]
                {
                    MonitorInfo::new(
                        monitor,
                        monitor_hdr_info_map
                            .get(
                                MonitorInfo::get_device_name(monitor)
                                    .unwrap_or_default()
                                    .as_str(),
                            )
                            .unwrap_or(&MonitorHdrInfo::default())
                            .clone(),
                    )
                }

                #[cfg(target_os = "macos")]
                {
                    MonitorInfo::new(monitor, MonitorHdrInfo::default())
                }
            })
            .filter(|monitor| monitor.rect.overlaps(&region))
            .collect::<Vec<MonitorInfo>>();

        MonitorList(monitor_info_list)
    }

    pub fn all() -> MonitorList {
        Self::get_monitors(None)
    }

    pub fn get_by_region(region: ElementRect) -> MonitorList {
        Self::get_monitors(Some(region))
    }

    /// 获取所有显示器的最小矩形
    pub fn get_monitors_bounding_box(&self) -> ElementRect {
        let monitors = &self.0;

        if monitors.is_empty() {
            return ElementRect {
                min_x: 0,
                min_y: 0,
                max_x: 0,
                max_y: 0,
            };
        }

        let mut min_x = i32::MAX;
        let mut min_y = i32::MAX;
        let mut max_x = i32::MIN;
        let mut max_y = i32::MIN;

        for monitor in monitors {
            if monitor.rect.min_x < min_x {
                min_x = monitor.rect.min_x;
            }
            if monitor.rect.min_y < min_y {
                min_y = monitor.rect.min_y;
            }
            if monitor.rect.max_x > max_x {
                max_x = monitor.rect.max_x;
            }
            if monitor.rect.max_y > max_y {
                max_y = monitor.rect.max_y;
            }
        }

        ElementRect {
            min_x,
            min_y,
            max_x,
            max_y,
        }
    }

    /// 捕获所有显示器，拼接为一个完整的图像
    ///
    /// @param crop_region 显示器的裁剪区域
    async fn capture_future(
        &self,
        crop_region: Option<ElementRect>,
        exclude_window: Option<&tauri::Window>,
        capture_option: CaptureOption,
    ) -> Result<image::DynamicImage, String> {
        let monitors = &self.0;

        // 特殊情况，只有一个显示器，直接返回
        if monitors.len() == 1 {
            let first_monitor = monitors.first().unwrap();
            let capture_image = first_monitor.capture(
                if let Some(crop_region) = crop_region {
                    Some(first_monitor.get_monitor_crop_region(crop_region))
                } else {
                    None
                },
                exclude_window,
                capture_option,
            );

            // 有些捕获失败的显示器，返回一个空图像，这里需要特殊处理
            if let Some(capture_image) = capture_image.as_ref() {
                if capture_image.width() == 1 && capture_image.height() == 1 {
                    return Ok(image::DynamicImage::new_rgb8(
                        (first_monitor.rect.max_x - first_monitor.rect.min_x) as u32,
                        (first_monitor.rect.max_y - first_monitor.rect.min_y) as u32,
                    ));
                }
            }

            return match capture_image {
                Some(capture_image) => Ok(capture_image),
                None => {
                    return Err(format!(
                        "[MonitorInfoList::capture] Failed to capture monitor image, monitor rect: {:?}",
                        first_monitor.rect
                    ));
                }
            };
        }

        // 将每个显示器截取的图像，绘制到该图像上
        let monitor_image_list = monitors
            .par_iter()
            .filter(|monitor| monitor.rect.overlaps(&crop_region.unwrap_or(ElementRect {
                min_x: i32::MIN,
                min_y: i32::MIN,
                max_x: i32::MAX,
                max_y: i32::MAX,
            })))
            .map(|monitor| {
                let monitor_crop_region = if let Some(crop_region) = crop_region {
                    Some(monitor.get_monitor_crop_region(crop_region))
                } else {
                    None
                };

                let capture_image = monitor.capture(monitor_crop_region, exclude_window, capture_option);

                match capture_image {
                    Some(image) => Some((image, monitor_crop_region)),
                    None => {
                        log::warn!(
                            "[MonitorInfoList::capture] Failed to capture monitor image, monitor rect: {:?}",
                            monitor.rect
                        );

                        None
                    }
                }
            })
            .filter_map(|result| match result {
                Some((image, monitor_crop_region)) => Some((image, monitor_crop_region)),
                None => None,
            })
            .collect::<Vec<(image::DynamicImage, Option<ElementRect>)>>();

        if monitor_image_list.is_empty() {
            return Err(format!(
                "[MonitorInfoList::capture] Failed to capture monitor image, monitor_image_list is empty, crop_region: {:?}",
                crop_region
            ));
        }

        // 获取能容纳所有显示器的最小矩形
        let monitors_bounding_box = self.get_monitors_bounding_box();

        // 声明该图像，分配内存
        let (capture_image_width, capture_image_height) = if let Some(crop_region) = crop_region {
            (
                (crop_region.max_x - crop_region.min_x) as usize,
                (crop_region.max_y - crop_region.min_y) as usize,
            )
        } else {
            (
                (monitors_bounding_box.max_x - monitors_bounding_box.min_x) as usize,
                (monitors_bounding_box.max_y - monitors_bounding_box.min_y) as usize,
            )
        };

        let pixel_len = match capture_option.color_format {
            ColorFormat::Rgb8 => 3,
            ColorFormat::Rgba8 => 4,
        };

        let mut capture_image_pixels: Vec<u8> =
            vec![0; capture_image_width * capture_image_height * pixel_len];

        let capture_image_pixels_ptr = capture_image_pixels.as_mut_ptr() as usize;

        monitor_image_list.par_iter().enumerate().for_each(
            |(index, (monitor_image, monitor_crop_region))| {
                let monitor = &monitors[index];

                // 计算显示器在合并图像中的位置
                let offset_x: i32;
                let offset_y: i32;

                if let Some(monitor_crop_region) = monitor_crop_region {
                    let crop_region = crop_region.unwrap();

                    // 将单个显示器的坐标转为整个显示器的坐标
                    // 得到图像相对整个显示器的坐标后，再减去裁剪区域的坐标，得到图像相对裁剪区域的坐标
                    offset_x = monitor_crop_region.min_x + monitor.rect.min_x - crop_region.min_x;
                    offset_y = monitor_crop_region.min_y + monitor.rect.min_y - crop_region.min_y;
                } else {
                    offset_x = monitor.rect.min_x - monitors_bounding_box.min_x;
                    offset_y = monitor.rect.min_y - monitors_bounding_box.min_y;
                }

                if offset_x < 0 || offset_y < 0 {
                    log::error!(
                        "[MonitorInfoList::capture] offset_x or offset_y is less than 0, offset_x: {:?}, offset_y: {:?}",
                        offset_x,
                        offset_y
                    );
                }

                // 将显示器图像绘制到合并图像上
                super::overlay_image_ptr(
                    capture_image_pixels_ptr as *mut u8,
                    capture_image_width,
                    monitor_image,
                    offset_x as usize,
                    offset_y as usize,
                    pixel_len,
                );
            },
        );

        let capture_image = match capture_option.color_format {
            ColorFormat::Rgb8 => image::DynamicImage::ImageRgb8(
                image::RgbImage::from_raw(
                    capture_image_width as u32,
                    capture_image_height as u32,
                    capture_image_pixels,
                )
                .unwrap(),
            ),
            ColorFormat::Rgba8 => image::DynamicImage::ImageRgba8(
                image::RgbaImage::from_raw(
                    capture_image_width as u32,
                    capture_image_height as u32,
                    capture_image_pixels,
                )
                .unwrap(),
            ),
        };

        Ok(capture_image)
    }

    /// 应用 5x5 颜色变换矩阵到 RGB 像素
    fn apply_color_matrix(pixel: *const u8, output_pixel: *mut u8, matrix: &[f32; 25]) {
        let (red_f, green_f, blue_f) = unsafe {
            (
                *pixel.add(0) as f32 / 255.0,
                *pixel.add(1) as f32 / 255.0,
                *pixel.add(2) as f32 / 255.0,
            )
        };

        // 前 3 行：矩阵乘法计算 RGB 变换（不处理 alpha 通道）
        unsafe {
            for i in 0..3 {
                let mut current_result = 0.0;

                // 处理 RGB 变换
                current_result += matrix[i * 5 + 0] * red_f; // 注意 current_output 未初始化
                current_result += matrix[i * 5 + 1] * green_f;
                current_result += matrix[i * 5 + 2] * blue_f;

                // 第5行提供平移（相加）操作：直接加上第5行对应的值
                current_result += matrix[4 * 5 + i];

                // 将结果限制在0.0-1.0范围内
                current_result = current_result.clamp(0.0, 1.0);

                output_pixel.add(i).write((current_result * 255.0) as u8);
            }
        }
    }

    /// 将颜色矩阵应用到整个图像
    fn apply_color_effect_to_image(
        image: &DynamicImage,
        matrix: &[f32; 25],
        color_format: ColorFormat,
    ) -> Result<DynamicImage, String> {
        let (width, height) = image.dimensions();

        let image_raw = image.as_bytes();
        let pixel_len = match color_format {
            ColorFormat::Rgba8 => 4,
            ColorFormat::Rgb8 => 3,
        };
        let mut output_data = unsafe {
            let mut array: Vec<u8> = Vec::with_capacity(image_raw.len());
            array.set_len(image_raw.len());

            array
        };
        let image_raw_ptr = image_raw.as_ptr() as usize;
        let output_data_ptr = output_data.as_mut_ptr() as usize;

        let pixel_count = (width * height) as usize;

        (0..pixel_count).into_par_iter().for_each(|pixel_index| {
            let index = pixel_index * pixel_len;
            unsafe {
                Self::apply_color_matrix(
                    (image_raw_ptr as *const u8).add(index),
                    (output_data_ptr as *mut u8).add(index),
                    matrix,
                );
            }
        });

        match color_format {
            ColorFormat::Rgba8 => Ok(DynamicImage::ImageRgba8(
                RgbaImage::from_raw(width, height, output_data).unwrap(),
            )),
            ColorFormat::Rgb8 => Ok(DynamicImage::ImageRgb8(
                RgbImage::from_raw(width, height, output_data).unwrap(),
            )),
        }
    }

    /**
     * 获取 Windows 下放大镜的颜色变换举证
     */
    async fn get_mag_color_effect() -> Result<Option<[f32; 25]>, String> {
        #[cfg(not(target_os = "windows"))]
        {
            return Ok(None);
        }

        #[cfg(target_os = "windows")]
        {
            let init_result = unsafe { windows::Win32::UI::Magnification::MagInitialize() };
            if !init_result.as_bool() {
                log::warn!(
                    "[MonitorInfoList::get_mag_color_effect] Failed to initialize magnification library"
                );
                return Ok(None);
            }

            let mut current_effect = windows::Win32::UI::Magnification::MAGCOLOREFFECT::default();
            let get_effect_result = unsafe {
                windows::Win32::UI::Magnification::MagGetFullscreenColorEffect(&mut current_effect)
            };

            // 释放 Mag
            let uninit_result = unsafe { windows::Win32::UI::Magnification::MagUninitialize() };
            if !uninit_result.as_bool() {
                log::warn!(
                    "[MonitorInfoList::get_mag_color_effect] Failed to uninitialize magnification library"
                );
            }

            if !get_effect_result.as_bool() {
                log::warn!(
                    "[MonitorInfoList::get_mag_color_effect] Failed to get magnification color effect"
                );
                return Ok(None);
            }

            let matrix = current_effect.transform;
            // 无任何效果的默认矩阵
            const NORMAL_MATRIX: [f32; 25] = [
                1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
            ];
            // 判断 matrix 是否等于 NORMAL_MATRIX
            if matrix.eq(&NORMAL_MATRIX) {
                return Ok(None);
            }

            Ok(Some(matrix))
        }
    }

    async fn capture_core(
        &self,
        crop_region: Option<ElementRect>,
        exclude_window: Option<&tauri::Window>,
        capture_option: CaptureOption,
    ) -> Result<image::DynamicImage, String> {
        let result = tokio::try_join!(
            self.capture_future(crop_region, exclude_window, capture_option,),
            Self::get_mag_color_effect()
        );

        match result {
            Ok((image, color_effect)) => {
                let image = match color_effect {
                    Some(matrix) => Self::apply_color_effect_to_image(
                        &image,
                        &matrix,
                        capture_option.color_format,
                    )?,
                    None => image,
                };

                Ok(image)
            }
            Err(e) => {
                log::error!("[MonitorInfoList::capture_core] failed to capture: {:?}", e);
                Err(e)
            }
        }
    }

    pub async fn capture(
        &self,
        exclude_window: Option<&tauri::Window>,
        capture_option: CaptureOption,
    ) -> Result<image::DynamicImage, String> {
        self.capture_core(None, exclude_window, capture_option)
            .await
    }

    pub async fn capture_region(
        &self,
        region: ElementRect,
        exclude_window: Option<&tauri::Window>,
        capture_option: CaptureOption,
    ) -> Result<image::DynamicImage, String> {
        self.capture_core(Some(region), exclude_window, capture_option)
            .await
    }

    pub fn monitor_rect_list(&self) -> Vec<MonitorRect> {
        self.0
            .iter()
            .map(|monitor| MonitorRect {
                rect: monitor.rect,
                scale_factor: monitor.scale_factor,
            })
            .collect()
    }

    pub fn iter(&self) -> impl Iterator<Item = &MonitorInfo> {
        self.0.iter()
    }
}

#[cfg(test)]
mod tests {
    use std::env;

    use super::*;

    #[cfg(target_os = "windows")]
    #[test]
    fn test_get_all_monitors() {
        use crate::monitor_hdr_info;

        let monitors = MonitorList::all();
        println!("monitors: {:?}", monitors);

        let monitor_hdr_info_map = monitor_hdr_info::get_all_monitors_sdr_info().unwrap();
        println!("monitor_hdr_info_map: {:?}", monitor_hdr_info_map);

        for monitor in monitors.iter() {
            println!(
                "monitor: {:?}",
                MonitorInfo::get_device_name(&monitor.monitor).unwrap()
            );
            println!(
                "monitor_hdr_info: {:?}",
                monitor_hdr_info_map
                    .get(
                        MonitorInfo::get_device_name(&monitor.monitor)
                            .unwrap()
                            .as_str()
                    )
                    .unwrap()
            );
        }
    }

    #[tokio::test]
    async fn test_capture_multi_monitor() {
        let instance = std::time::Instant::now();

        let crop_region: ElementRect;
        #[cfg(target_os = "windows")]
        {
            crop_region = ElementRect {
                min_x: -3840,
                min_y: 0,
                max_x: 3840,
                max_y: 2160,
            };
        }
        #[cfg(target_os = "macos")]
        {
            crop_region = ElementRect {
                min_x: 0,
                min_y: 0,
                max_x: 7680,
                max_y: 2160,
            };
        }

        let monitors = MonitorList::get_by_region(crop_region);

        let image = monitors
            .capture(
                None,
                CaptureOption {
                    color_format: ColorFormat::Rgb8,
                    correct_hdr_color_algorithm: CorrectHdrColorAlgorithm::None,
                },
            )
            .await
            .unwrap();

        println!("current_dir: {:?}", env::current_dir().unwrap());

        image
            .save(
                std::path::PathBuf::from(env::current_dir().unwrap())
                    .join("../../test_output/capture_multi_monitor.webp"),
            )
            .unwrap();

        println!("time: {:?}", instance.elapsed());
    }

    #[tokio::test]
    async fn test_capture_single_monitor() {
        let instance = std::time::Instant::now();

        let crop_region = ElementRect {
            min_x: 0,
            min_y: 0,
            max_x: 1000,
            max_y: 1000,
        };

        let monitors = MonitorList::get_by_region(crop_region);
        let image = monitors
            .capture_region(
                crop_region,
                None,
                CaptureOption {
                    color_format: ColorFormat::Rgb8,
                    correct_hdr_color_algorithm: CorrectHdrColorAlgorithm::None,
                },
            )
            .await
            .unwrap();

        image
            .save(
                std::path::PathBuf::from(env::current_dir().unwrap())
                    .join("../../test_output/capture_single_monitor.webp"),
            )
            .unwrap();

        println!("time: {:?}", instance.elapsed());
    }
}
