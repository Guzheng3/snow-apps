mod fill;

use rapid_ocr_rs::{
    DictionarySource, EngineConfig, LangDet, LangRec, ModelSource, ModelType, OcrCallOptions,
    OcrInput, OcrResult, OcrVersion, PipelineSources, Quad, RapidOcrEngine,
    initialize_onnx_runtime,
};
use std::{
    cell::RefCell,
    ffi::{CString, c_char},
    mem::size_of,
    ptr, slice,
    sync::OnceLock,
};

const DETECTOR_BYTES: &[u8] = include_bytes!("../assets/ppocrv6-small/PP-OCRv6_det_small.onnx");
const RECOGNIZER_BYTES: &[u8] = include_bytes!("../assets/ppocrv6-small/PP-OCRv6_rec_small.onnx");
const DICTIONARY_TEXT: &str = include_str!("../assets/ppocrv6-small/ppocrv6_dict.txt");
static ONNX_RUNTIME_INITIALIZATION: OnceLock<Result<(), String>> = OnceLock::new();

thread_local! {
    static LAST_ERROR: RefCell<CString> = RefCell::new(CString::new("").expect("empty C string"));
}

#[repr(C)]
pub struct SnowOcrRequestV1 {
    pub struct_size: u32,
    pub width: u32,
    pub height: u32,
    pub stride_bytes: u32,
    pub rgba_bytes: *const u8,
    pub rgba_len: usize,
}

#[repr(C)]
pub struct SnowOcrImageInfoV1 {
    pub struct_size: u32,
    pub width: u32,
    pub height: u32,
    pub stride_bytes: u32,
    pub rgba_bytes: *const u8,
    pub rgba_len: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SnowOcrQuad {
    pub points: [f32; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SnowOcrColor {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
    pub alpha: u8,
}

#[repr(C)]
pub struct SnowOcrLineInfoV1 {
    pub struct_size: u32,
    pub text_utf8: *const u8,
    pub text_len: usize,
    pub confidence: f32,
    pub quad: SnowOcrQuad,
    pub foreground: SnowOcrColor,
}

pub struct SnowOcrEngine {
    engine: RapidOcrEngine,
}

pub struct SnowOcrResult {
    width: u32,
    height: u32,
    rgba: Vec<u8>,
    lines: Vec<OwnedLine>,
}

struct OwnedLine {
    text: Vec<u8>,
    confidence: f32,
    quad: Quad,
    foreground: [u8; 4],
}

fn set_last_error(error: impl ToString) {
    let text = error.to_string().replace('\0', " ");
    LAST_ERROR.with(|slot| {
        *slot.borrow_mut() = CString::new(text).expect("NUL bytes were removed");
    });
}

fn clear_last_error() {
    set_last_error("");
}

fn create_engine() -> rapid_ocr_rs::Result<RapidOcrEngine> {
    ensure_onnx_runtime().map_err(rapid_ocr_rs::RapidOcrError::Config)?;
    let mut config = EngineConfig::default();
    config.global.use_det = true;
    config.global.use_cls = false;
    config.global.use_rec = true;
    config.det.lang = LangDet::Multi;
    config.det.ocr_version = OcrVersion::PPocrV6;
    config.det.model_type = ModelType::Small;
    config.det.allow_download = false;
    config.rec.model.lang = LangRec::Ch;
    config.rec.model.ocr_version = OcrVersion::PPocrV6;
    config.rec.model.model_type = ModelType::Small;
    config.rec.model.allow_download = false;

    RapidOcrEngine::new_with_sources(
        config,
        PipelineSources {
            det: Some(ModelSource::Memory {
                name: "embedded:PP-OCRv6_det_small.onnx",
                bytes: DETECTOR_BYTES,
            }),
            cls: None,
            rec: Some(ModelSource::Memory {
                name: "embedded:PP-OCRv6_rec_small.onnx",
                bytes: RECOGNIZER_BYTES,
            }),
            rec_dictionary: Some(DictionarySource::Memory {
                name: "embedded:ppocrv6_dict.txt",
                text: DICTIONARY_TEXT,
            }),
        },
    )
}

fn ensure_onnx_runtime() -> Result<(), String> {
    ONNX_RUNTIME_INITIALIZATION
        .get_or_init(|| {
            initialize_onnx_runtime()
                .map_err(|error| format!("unable to initialize ONNX Runtime: {error}"))
        })
        .clone()
}

#[unsafe(no_mangle)]
pub extern "C" fn snow_ocr_engine_create() -> *mut SnowOcrEngine {
    match create_engine() {
        Ok(engine) => {
            clear_last_error();
            Box::into_raw(Box::new(SnowOcrEngine { engine }))
        }
        Err(error) => {
            set_last_error(error);
            ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// `engine` must be null or a live handle returned by `snow_ocr_engine_create`.
pub unsafe extern "C" fn snow_ocr_engine_destroy(engine: *mut SnowOcrEngine) {
    if !engine.is_null() {
        drop(unsafe { Box::from_raw(engine) });
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// The request and its pixel buffer must remain readable for the duration of
/// the call. The engine must be live and exclusively accessed.
pub unsafe extern "C" fn snow_ocr_engine_recognize_rgba(
    engine: *mut SnowOcrEngine,
    request: *const SnowOcrRequestV1,
) -> *mut SnowOcrResult {
    let Some(engine) = (unsafe { engine.as_mut() }) else {
        set_last_error("OCR engine is null");
        return ptr::null_mut();
    };
    let Some(request) = (unsafe { request.as_ref() }) else {
        set_last_error("OCR request is null");
        return ptr::null_mut();
    };
    if request.struct_size as usize != size_of::<SnowOcrRequestV1>() {
        set_last_error("OCR request has an incompatible struct size");
        return ptr::null_mut();
    }

    match recognize(engine, request) {
        Ok(result) => {
            clear_last_error();
            Box::into_raw(Box::new(result))
        }
        Err(error) => {
            set_last_error(error);
            ptr::null_mut()
        }
    }
}

fn recognize(
    engine: &mut SnowOcrEngine,
    request: &SnowOcrRequestV1,
) -> Result<SnowOcrResult, String> {
    let width = request.width as usize;
    let height = request.height as usize;
    let stride = request.stride_bytes as usize;
    let row_bytes = width.checked_mul(4).ok_or("OCR row size overflow")?;
    let required = stride
        .checked_mul(height)
        .ok_or("OCR image buffer size overflow")?;
    if width == 0 || height == 0 || stride < row_bytes || request.rgba_bytes.is_null() {
        return Err("OCR image dimensions, stride, or pixel pointer are invalid".to_string());
    }
    if request.rgba_len < required {
        return Err("OCR image buffer is shorter than its declared dimensions".to_string());
    }

    let source = unsafe { slice::from_raw_parts(request.rgba_bytes, required) };
    let mut rgba = Vec::with_capacity(row_bytes * height);
    for row in source.chunks(stride).take(height) {
        rgba.extend_from_slice(&row[..row_bytes]);
    }
    let result = engine
        .engine
        .run(
            OcrInput::RgbaU8 {
                width,
                height,
                data: rgba.clone(),
            },
            OcrCallOptions {
                use_det: Some(true),
                use_cls: Some(false),
                use_rec: Some(true),
                ..OcrCallOptions::default()
            },
        )
        .map_err(|error| error.to_string())?;

    let mut lines = match result {
        OcrResult::Empty => Vec::new(),
        OcrResult::Full(full) => full
            .boxes
            .into_iter()
            .zip(full.lines)
            .map(|(quad, line)| OwnedLine {
                text: line.text.into_bytes(),
                confidence: line.score,
                quad,
                foreground: [0, 0, 0, 255],
            })
            .collect(),
        _ => return Err("OCR pipeline returned an incomplete result".to_string()),
    };

    let line_quads = lines.iter().map(|line| line.quad).collect::<Vec<_>>();
    let foregrounds = fill::white_blur_fill(&mut rgba, width, height, &line_quads);
    for (line, foreground) in lines.iter_mut().zip(foregrounds) {
        line.foreground = foreground;
    }

    Ok(SnowOcrResult {
        width: request.width,
        height: request.height,
        rgba,
        lines,
    })
}

fn ffi_quad(quad: Quad) -> SnowOcrQuad {
    SnowOcrQuad {
        points: [
            quad[0][0], quad[0][1], quad[1][0], quad[1][1], quad[2][0], quad[2][1], quad[3][0],
            quad[3][1],
        ],
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// `result` must be null or a live result returned by the recognize function.
pub unsafe extern "C" fn snow_ocr_result_destroy(result: *mut SnowOcrResult) {
    if !result.is_null() {
        drop(unsafe { Box::from_raw(result) });
    }
}

#[unsafe(no_mangle)]
/// # Safety
/// The result must be live and `out_image` must be writable.
pub unsafe extern "C" fn snow_ocr_result_image(
    result: *const SnowOcrResult,
    out_image: *mut SnowOcrImageInfoV1,
) -> u8 {
    let (Some(result), Some(out_image)) = (
        (unsafe { result.as_ref() }),
        (unsafe { out_image.as_mut() }),
    ) else {
        set_last_error("OCR result or image output is null");
        return 0;
    };
    if out_image.struct_size as usize != size_of::<SnowOcrImageInfoV1>() {
        set_last_error("OCR image output has an incompatible struct size");
        return 0;
    }
    *out_image = SnowOcrImageInfoV1 {
        struct_size: size_of::<SnowOcrImageInfoV1>() as u32,
        width: result.width,
        height: result.height,
        stride_bytes: result.width * 4,
        rgba_bytes: result.rgba.as_ptr(),
        rgba_len: result.rgba.len(),
    };
    clear_last_error();
    1
}

#[unsafe(no_mangle)]
/// # Safety
/// `result` must be null or a live result.
pub unsafe extern "C" fn snow_ocr_result_line_count(result: *const SnowOcrResult) -> usize {
    unsafe { result.as_ref() }.map_or(0, |result| result.lines.len())
}

#[unsafe(no_mangle)]
/// # Safety
/// The result must be live and `out_line` must be writable.
pub unsafe extern "C" fn snow_ocr_result_line(
    result: *const SnowOcrResult,
    line_index: usize,
    out_line: *mut SnowOcrLineInfoV1,
) -> u8 {
    let (Some(result), Some(out_line)) =
        ((unsafe { result.as_ref() }), (unsafe { out_line.as_mut() }))
    else {
        set_last_error("OCR result or line output is null");
        return 0;
    };
    if out_line.struct_size as usize != size_of::<SnowOcrLineInfoV1>() {
        set_last_error("OCR line output has an incompatible struct size");
        return 0;
    }
    let Some(line) = result.lines.get(line_index) else {
        set_last_error("OCR line index is out of range");
        return 0;
    };
    *out_line = SnowOcrLineInfoV1 {
        struct_size: size_of::<SnowOcrLineInfoV1>() as u32,
        text_utf8: line.text.as_ptr(),
        text_len: line.text.len(),
        confidence: line.confidence,
        quad: ffi_quad(line.quad),
        foreground: SnowOcrColor {
            red: line.foreground[0],
            green: line.foreground[1],
            blue: line.foreground[2],
            alpha: line.foreground[3],
        },
    };
    clear_last_error();
    1
}

#[unsafe(no_mangle)]
pub extern "C" fn snow_ocr_last_error_message() -> *const c_char {
    LAST_ERROR.with(|slot| slot.borrow().as_ptr())
}

#[cfg(test)]
mod tests {
    use super::*;
    use sha2::{Digest, Sha256};

    #[test]
    fn embedded_assets_match_pinned_hashes() {
        assert_eq!(DETECTOR_BYTES.len(), 9_929_594);
        assert_eq!(RECOGNIZER_BYTES.len(), 21_234_383);
        assert_eq!(DICTIONARY_TEXT.len(), 74_947);
        assert_eq!(
            format!("{:x}", Sha256::digest(DETECTOR_BYTES)),
            "090f04abcd9d9a7498bc4ebf677e4cb9bdce1fe4197ddb7e529f1ef44e1ff94f"
        );
        assert_eq!(
            format!("{:x}", Sha256::digest(RECOGNIZER_BYTES)),
            "6f327246b50388f3c176ae304bd95767ea6dc0c9ae92153ef8cbe210b3c14884"
        );
        assert_eq!(
            format!("{:x}", Sha256::digest(DICTIONARY_TEXT.as_bytes())),
            "b5f2bfe2bdd9448429e3e82b51c789775d9b42f2403d082b00662eb77e401c5d"
        );
    }

    #[test]
    fn ffi_result_exposes_complete_lines() {
        let result = SnowOcrResult {
            width: 1,
            height: 1,
            rgba: vec![0, 0, 0, 255],
            lines: vec![OwnedLine {
                text: b"one complete line".to_vec(),
                confidence: 0.9,
                quad: [[1.0, 2.0], [9.0, 2.0], [9.0, 6.0], [1.0, 6.0]],
                foreground: [12, 34, 56, 255],
            }],
        };
        let mut line = SnowOcrLineInfoV1 {
            struct_size: size_of::<SnowOcrLineInfoV1>() as u32,
            text_utf8: ptr::null(),
            text_len: 0,
            confidence: 0.0,
            quad: SnowOcrQuad::default(),
            foreground: SnowOcrColor::default(),
        };

        assert_eq!(unsafe { snow_ocr_result_line_count(&result) }, 1);
        assert_eq!(unsafe { snow_ocr_result_line(&result, 0, &mut line) }, 1);
        assert_eq!(
            unsafe { slice::from_raw_parts(line.text_utf8, line.text_len) },
            b"one complete line"
        );
        assert_eq!(line.quad.points, [1.0, 2.0, 9.0, 2.0, 9.0, 6.0, 1.0, 6.0]);
    }

    #[test]
    fn embedded_pipeline_recognizes_an_rgba_image() {
        let mut engine = SnowOcrEngine {
            engine: create_engine().expect("embedded OCR engine should initialize"),
        };
        let rgba = vec![255_u8; 64 * 64 * 4];
        let request = SnowOcrRequestV1 {
            struct_size: size_of::<SnowOcrRequestV1>() as u32,
            width: 64,
            height: 64,
            stride_bytes: 64 * 4,
            rgba_bytes: rgba.as_ptr(),
            rgba_len: rgba.len(),
        };
        let result = recognize(&mut engine, &request).expect("embedded OCR pipeline should run");
        assert_eq!((result.width, result.height), (64, 64));
        assert_eq!(result.rgba.len(), rgba.len());
    }
}
