use snow_draw_engine::{ElementId, Point, TextLayoutSize};

use crate::abi::convert::*;
use crate::abi::exports::snow_changed_viewports_destroy;
use crate::abi::handles::*;
use crate::abi::text::{active_text_draft_from_c, text_draft_commit_from_c, text_string_from_raw};
use crate::abi::types::*;

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `text_utf8` must either be null with `text_utf8_len == 0`, or point to `text_utf8_len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_create_text(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    center_x: f64,
    center_y: f64,
    text_utf8: *const std::ffi::c_char,
    text_utf8_len: u32,
    measured_width: f64,
    measured_height: f64,
) -> SnowError {
    ffi_error(|| {
        let mut changed_viewports = std::ptr::null_mut();
        let error = unsafe {
            snow_viewport_create_text_ex(
                runtime,
                viewport,
                center_x,
                center_y,
                text_utf8,
                text_utf8_len,
                measured_width,
                measured_height,
                &mut changed_viewports,
            )
        };
        unsafe {
            snow_changed_viewports_destroy(changed_viewports);
        }
        error
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `text_utf8` must either be null with `text_utf8_len == 0`, or point to `text_utf8_len` readable bytes.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_create_text_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    center_x: f64,
    center_y: f64,
    text_utf8: *const std::ffi::c_char,
    text_utf8_len: u32,
    measured_width: f64,
    measured_height: f64,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }
        let text = match text_string_from_raw(text_utf8, text_utf8_len) {
            Ok(text) => text,
            Err(error) => return error,
        };

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let result = state
                .runtime
                .create_text_with_viewport_changes(
                    id,
                    Point::new(center_x, center_y),
                    text,
                    TextLayoutSize {
                        width: measured_width,
                        height: measured_height,
                    },
                )
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_id` and `out_hit` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_hit_text(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    canvas_x: f64,
    canvas_y: f64,
    out_id: *mut SnowElementId,
    out_hit: *mut u8,
) -> SnowError {
    ffi_error(|| {
        if out_id.is_null() || out_hit.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, id| {
                if let Some(text_id) = runtime
                    .hit_text_at(id, Point::new(canvas_x, canvas_y))
                    .map_err(SnowError::from)?
                {
                    write_out(out_id, snow_element_id_from_rust(text_id));
                    write_out(out_hit, 1);
                } else {
                    write_out(out_id, SnowElementId::default());
                    write_out(out_hit, 0);
                }
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_count` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_selected_text_count(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    out_count: *mut u32,
) -> SnowError {
    ffi_error(|| {
        if out_count.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, _| {
                write_out(out_count, runtime.selected_text_count() as u32);
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_items` must be valid for writes of `capacity` items when `capacity > 0`.
/// `out_count` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_get_selected_text_elements(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    out_items: *mut SnowTextElementInfo,
    capacity: u32,
    out_count: *mut u32,
) -> SnowError {
    ffi_error(|| {
        if out_count.is_null() || (out_items.is_null() && capacity != 0) {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, _| {
                let items = runtime.selected_text_elements();
                write_out(out_count, items.len() as u32);
                if capacity != 0 {
                    let out_slice =
                        unsafe { std::slice::from_raw_parts_mut(out_items, capacity as usize) };
                    for (target, item) in out_slice.iter_mut().zip(items.into_iter()) {
                        *target = snow_text_element_info_from_rust(item);
                    }
                }
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_info` and `out_active` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_get_active_text_resize_measurement(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    out_info: *mut SnowTextElementInfo,
    out_active: *mut u8,
) -> SnowError {
    ffi_error(|| {
        if out_info.is_null() || out_active.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, id| {
                let request = runtime
                    .active_text_resize_measurement_request(id)
                    .map_err(SnowError::from)?;
                write_out(out_active, u8::from(request.is_some()));
                if let Some(info) = request {
                    write_out(out_info, snow_text_element_info_from_rust(info));
                }
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `layout` must point to a readable `SnowTextLayoutSize`, and `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_apply_active_text_resize_measurement_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    layout: *const SnowTextLayoutSize,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if layout.is_null() || out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let result = state
                .runtime
                .apply_active_text_resize_measurement_with_viewport_changes(id, unsafe {
                    (*layout).into()
                })
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `draft` must point to a readable `SnowActiveTextDraftPresentation`.
/// `draft.text_utf8` must either be null with `draft.text_utf8_len == 0`, or point to
/// `draft.text_utf8_len` readable bytes.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_set_active_text_draft_presentation_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    draft: *const SnowActiveTextDraftPresentation,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if draft.is_null() || out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        let draft = match active_text_draft_from_c(unsafe { &*draft }) {
            Ok(draft) => draft,
            Err(error) => return error,
        };
        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let result = state
                .runtime
                .set_active_text_draft_presentation_with_viewport_changes(id, draft)
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_clear_active_text_draft_presentation_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let result = state
                .runtime
                .clear_active_text_draft_presentation_with_viewport_changes(id)
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_info`, `out_style`, and `out_active` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_get_active_text_draft_presentation(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    out_info: *mut SnowTextElementInfo,
    out_style: *mut SnowTextStyle,
    out_active: *mut u8,
) -> SnowError {
    ffi_error(|| {
        if out_info.is_null() || out_style.is_null() || out_active.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, id| {
                let draft = runtime
                    .active_text_draft_presentation(id)
                    .map_err(SnowError::from)?;
                write_out(out_active, u8::from(draft.is_some()));
                if let Some((info, style)) = draft {
                    write_out(out_info, snow_text_element_info_from_rust(info));
                    write_out(out_style, style.into());
                }
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_bound` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_is_text_bound_to_serial_number(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    id: SnowElementId,
    out_bound: *mut u8,
) -> SnowError {
    ffi_error(|| {
        if out_bound.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_viewport_ref(
            runtime,
            viewport,
            |runtime, _| {
                let bound = runtime.is_text_bound_to_serial_number(ElementId {
                    index: id.index,
                    generation: id.generation,
                });
                write_out(out_bound, u8::from(bound));
                Ok(())
            },
        ))
    })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by this library.
/// `out_info` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_get_text_element(
    runtime: SnowRuntime,
    id: SnowElementId,
    out_info: *mut SnowTextElementInfo,
) -> SnowError {
    ffi_error(|| {
        if out_info.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_ref(runtime, |runtime| {
            let info = runtime
                .text_element_info(ElementId {
                    index: id.index,
                    generation: id.generation,
                })
                .map_err(SnowError::from)?;
            write_out(out_info, snow_text_element_info_from_rust(info));
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `text_utf8` must either be null with `text_utf8_len == 0`, or point to `text_utf8_len` readable bytes.
/// `out_changed_viewports` must be valid for writes.
///
/// This is the legacy unstyled commit path. It intentionally preserves the
/// historical behavior of committing content/layout through the editor default
/// style instead of accepting a full draft style payload.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_commit_text_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    existing_id: SnowElementId,
    has_existing_id: u8,
    center_x: f64,
    center_y: f64,
    text_utf8: *const std::ffi::c_char,
    text_utf8_len: u32,
    measured_width: f64,
    measured_height: f64,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }
        let text = match text_string_from_raw(text_utf8, text_utf8_len) {
            Ok(text) => text,
            Err(error) => return error,
        };

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let existing = (has_existing_id != 0).then_some(ElementId {
                index: existing_id.index,
                generation: existing_id.generation,
            });
            let result = state
                .runtime
                .commit_text_element_with_viewport_changes(
                    id,
                    existing,
                    Point::new(center_x, center_y),
                    text,
                    TextLayoutSize {
                        width: measured_width,
                        height: measured_height,
                    },
                )
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `draft` must point to a readable `SnowTextCommitDraft`.
/// `draft.text_utf8` must either be null with `draft.text_utf8_len == 0`, or point to
/// `draft.text_utf8_len` readable bytes.
/// `out_changed_viewports` must be valid for writes.
///
/// This is the typed styled draft commit path. It persists the supplied text,
/// exact host layout, auto-resize state, and full text style.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_commit_text_draft_payload_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    draft: *const SnowTextCommitDraft,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if draft.is_null() || out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        let draft = match text_draft_commit_from_c(unsafe { &*draft }) {
            Ok(draft) => draft,
            Err(error) => return error,
        };
        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let result = state
                .runtime
                .commit_text_draft_with_viewport_changes(id, draft)
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `text_utf8` must either be null with `text_utf8_len == 0`, or point to `text_utf8_len` readable bytes.
/// `style` must point to a readable `SnowTextStyle`.
/// `out_changed_viewports` must be valid for writes.
///
/// Convenience wrapper for `snow_viewport_commit_text_draft_payload_ex`; unlike
/// `snow_viewport_commit_text_ex`, this is a styled draft commit.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_commit_text_draft_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    existing_id: SnowElementId,
    has_existing_id: u8,
    center_x: f64,
    center_y: f64,
    text_utf8: *const std::ffi::c_char,
    text_utf8_len: u32,
    measured_width: f64,
    measured_height: f64,
    style: *const SnowTextStyle,
    auto_resize: u8,
    update_default_style: u8,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null()
            || style.is_null()
            || (text_utf8.is_null() && text_utf8_len != 0)
        {
            return SnowError::InvalidArgument;
        }

        let draft = SnowTextCommitDraft {
            element_id: existing_id,
            has_existing_element: has_existing_id,
            auto_resize,
            update_default_style,
            reserved0: [0; 5],
            center_x,
            center_y,
            text_utf8,
            text_utf8_len,
            reserved1: 0,
            measured_layout: SnowTextLayoutSize {
                width: measured_width,
                height: measured_height,
            },
            style: unsafe { *style },
        };
        unsafe {
            snow_viewport_commit_text_draft_payload_ex(
                runtime,
                viewport,
                &draft,
                out_changed_viewports,
            )
        }
    })
}

/// # Safety
/// If `runtime` and `viewport` are non-null, they must be live handles created by this library.
/// `out_text_id`, `out_has_text_id`, and `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_viewport_create_serial_number_text_ex(
    runtime: SnowRuntime,
    viewport: SnowViewport,
    measured_width: f64,
    measured_height: f64,
    out_text_id: *mut SnowElementId,
    out_has_text_id: *mut u8,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_text_id.is_null() || out_has_text_id.is_null() || out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let id = viewport_id(viewport)?;
            let (result, text_id) = state
                .runtime
                .create_serial_number_text_with_viewport_changes(
                    id,
                    TextLayoutSize {
                        width: measured_width,
                        height: measured_height,
                    },
                )
                .map_err(SnowError::from)?;
            if let Some(text_id) = text_id {
                write_out(out_text_id, snow_element_id_from_rust(text_id));
                write_out(out_has_text_id, 1);
            } else {
                write_out(out_text_id, SnowElementId::default());
                write_out(out_has_text_id, 0);
            }
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}
