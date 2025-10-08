use half::prelude::f16;
use rayon::iter::{IntoParallelIterator, ParallelIterator};
use snow_shot_app_shared::ElementRect;
use std::sync::mpsc::{Sender, channel};
use windows_capture::capture::{Context, GraphicsCaptureApiHandler};
use windows_capture::frame::Frame;
use windows_capture::graphics_capture_api::InternalCaptureControl;
use windows_capture::monitor::Monitor;
use windows_capture::settings::{
    ColorFormat, CursorCaptureSettings, DirtyRegionSettings, DrawBorderSettings,
    MinimumUpdateIntervalSettings, SecondaryWindowSettings, Settings,
};

use crate::monitor_info::MonitorInfo;

struct CaptureFlags {
    on_frame_arrived: Sender<(Vec<u8>, usize, usize)>,
    crop_area: Option<ElementRect>,
}

struct WindowsCaptureImage {
    capture_info: Option<CaptureFlags>,
}

impl GraphicsCaptureApiHandler for WindowsCaptureImage {
    type Flags = CaptureFlags;
    type Error = String;

    fn new(ctx: Context<Self::Flags>) -> Result<Self, Self::Error> {
        Ok(Self {
            capture_info: Some(ctx.flags),
        })
    }

    fn on_frame_arrived(
        &mut self,
        frame: &mut Frame,
        capture_control: InternalCaptureControl,
    ) -> Result<(), Self::Error> {
        capture_control.stop();

        let capture_info = match self.capture_info.take() {
            Some(capture_info) => capture_info,
            None => {
                return Err(format!(
                    "[WindowsCaptureImage::on_frame_arrived] capture_info is None"
                ));
            }
        };

        // Rgba16F 每个像素占用 8 个字节
        let mut origin_image = frame.buffer().unwrap();

        let origin_image_width = origin_image.width() as usize;
        let origin_image_height = origin_image.height() as usize;

        let orgin_image_buffer = origin_image.as_raw_buffer();

        let (min_x, min_y, max_x, max_y) = if let Some(crop_area) = capture_info.crop_area {
            (
                crop_area.min_x,
                crop_area.min_y,
                crop_area.max_x,
                crop_area.max_y,
            )
        } else {
            (0, 0, origin_image_width as i32, origin_image_height as i32)
        };

        let origin_x_offset = min_x as usize;
        let origin_y_offset = min_y as usize;
        let crop_width = (max_x - min_x) as usize;
        let crop_height = (max_y - min_y) as usize;
        let pixels_count = crop_width * crop_height;

        let pixel_byte_count = 8;
        let mut pixels: Vec<u8> = unsafe {
            let mut pixels = Vec::with_capacity(pixels_count * pixel_byte_count);
            pixels.set_len(pixels_count * pixel_byte_count);
            pixels
        };

        let origin_image_buffer_base_index =
            (origin_y_offset * origin_image_width + origin_x_offset) * pixel_byte_count;
        let origin_image_buffer_ptr = orgin_image_buffer.as_ptr() as usize;
        let pixels_ptr = pixels.as_mut_ptr() as usize;
        (0..crop_height).into_par_iter().for_each(|y| {
            let origin_image_index =
                origin_image_buffer_base_index + y * origin_image_width * pixel_byte_count;
            let target_image_index = y * crop_width * pixel_byte_count;

            unsafe {
                std::ptr::copy_nonoverlapping(
                    (origin_image_buffer_ptr as *const u8).add(origin_image_index),
                    (pixels_ptr as *mut u8).add(target_image_index),
                    crop_width * pixel_byte_count,
                );
            }
        });

        match capture_info
            .on_frame_arrived
            .send((pixels, crop_width, crop_height))
        {
            Ok(()) => Ok(()),
            Err(_) => {
                log::error!("[WindowsCaptureImage::on_frame_arrived] failed to send pixels");

                Err(format!(
                    "[WindowsCaptureImage::on_frame_arrived] failed to send pixels"
                ))
            }
        }
    }

    fn on_closed(&mut self) -> Result<(), Self::Error> {
        Ok(())
    }
}

/// 将线性颜色值转换为 sRGB 颜色值
#[inline]
fn linear_to_srgb_byte(linear: f32) -> u8 {
    let srgb = if linear <= 0.0031308 {
        12.92 * linear
    } else {
        1.055 * linear.powf(1.0 / 2.4) - 0.055
    };

    if srgb < 0.0 {
        0
    } else if srgb > 1.0 {
        255
    } else {
        (srgb * 255.0) as u8
    }
}

#[inline]
pub fn write_rgba16f_linear_to_rgb8(
    rgba16f_image: *const u8,
    rgb8_image: *mut u8,
    hdr_scale: f32,
    pixel_index: usize,
) {
    unsafe {
        let red_f = f16::from_bits(u16::from_le(
            *(rgba16f_image.add(pixel_index * 8) as *const u16),
        ))
        .to_f32()
            * hdr_scale;
        let green_f = f16::from_bits(u16::from_le(
            *(rgba16f_image.add(pixel_index * 8 + 2) as *const u16),
        ))
        .to_f32()
            * hdr_scale;
        let blue_f = f16::from_bits(u16::from_le(
            *(rgba16f_image.add(pixel_index * 8 + 4) as *const u16),
        ))
        .to_f32()
            * hdr_scale;

        // 使用快速饱和转换
        rgb8_image
            .add(pixel_index * 3)
            .write(linear_to_srgb_byte(red_f));
        rgb8_image
            .add(pixel_index * 3 + 1)
            .write(linear_to_srgb_byte(green_f));
        rgb8_image
            .add(pixel_index * 3 + 2)
            .write(linear_to_srgb_byte(blue_f));
    }
}

pub fn capture_monitor_image(
    monitor: &MonitorInfo,
    crop_area: Option<ElementRect>,
) -> Result<image::DynamicImage, String> {
    let capture_monitor =
        Monitor::from_raw_hmonitor(MonitorInfo::get_monitor_handle(&monitor.monitor).0);

    let (sender, receiver) = channel();

    let settings = Settings::new(
        capture_monitor,
        CursorCaptureSettings::WithoutCursor,
        DrawBorderSettings::WithoutBorder,
        SecondaryWindowSettings::Default,
        MinimumUpdateIntervalSettings::Default,
        DirtyRegionSettings::Default,
        ColorFormat::Rgba16F,
        CaptureFlags {
            on_frame_arrived: sender,
            crop_area,
        },
    );

    match WindowsCaptureImage::start(settings) {
        Ok(capturer) => capturer,
        Err(e) => {
            return Err(format!(
                "[windows_capture_image::capture_monitor_image] failed to start capturer: {:?}",
                e
            ));
        }
    };

    let (rgba16f_image, image_width, image_height) = match receiver.recv() {
        Ok(image) => image,
        Err(e) => {
            return Err(format!(
                "[windows_capture_image::capture_monitor_image] failed to receive image: {:?}",
                e
            ));
        }
    };

    let rgb8_image_pixels_count = image_width * image_height;
    let mut rgb8_image_pixels: Vec<u8> = unsafe {
        let mut rgb8_image_pixels = Vec::with_capacity(rgb8_image_pixels_count * 3);
        rgb8_image_pixels.set_len(rgb8_image_pixels_count * 3);
        rgb8_image_pixels
    };

    let hdr_scale = 1000.0 / (monitor.monitor_hdr_info.sdr_white_level as f32);

    let rgb8_image_pixels_ptr = rgb8_image_pixels.as_mut_ptr() as usize;
    let rgba16f_image_ptr = rgba16f_image.as_ptr() as usize;
    (0..rgb8_image_pixels_count).into_par_iter().for_each(|i| {
        write_rgba16f_linear_to_rgb8(
            rgba16f_image_ptr as *const u8,
            rgb8_image_pixels_ptr as *mut u8,
            hdr_scale,
            i,
        );
    });

    let rgb8_image =
        match image::RgbImage::from_raw(image_width as u32, image_height as u32, rgb8_image_pixels)
        {
            Some(rgb8_image) => rgb8_image,
            None => {
                return Err(format!(
                    "[windows_capture_image::capture_monitor_image] Failed to create rgb8 image"
                ));
            }
        };

    Ok(image::DynamicImage::ImageRgb8(rgb8_image))
}
