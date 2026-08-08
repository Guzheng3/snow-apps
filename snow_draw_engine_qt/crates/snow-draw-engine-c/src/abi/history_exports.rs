use crate::abi::exports::snow_changed_viewports_destroy;
use crate::abi::handles::*;
use crate::abi::types::*;

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
/// `out_state` must be valid for writes of one `SnowHistoryState` value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_get_history_state(
    runtime: SnowRuntime,
    out_state: *mut SnowHistoryState,
) -> SnowError {
    ffi_error(|| {
        if out_state.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_ref(runtime, |runtime| {
            write_out(out_state, runtime.history_state().into());
            Ok(())
        }))
    })
}

/// # Safety
/// If `engine` is non-null, it must be a live handle returned by `snow_engine_create`.
/// `out_state` must be valid for writes of one `SnowHistoryState` value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_engine_get_history_state(
    engine: SnowEngine,
    out_state: *mut SnowHistoryState,
) -> SnowError {
    ffi_error(|| unsafe { snow_runtime_get_history_state(engine, out_state) })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_clear_document_preserving_viewports(
    runtime: SnowRuntime,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let result = state
                .runtime
                .clear_document_preserving_viewports()
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
/// `bytes` must point to `size` readable bytes containing a serialized document history.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_restore_document_history_preserving_editor_styles(
    runtime: SnowRuntime,
    bytes: *const u8,
    size: usize,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if bytes.is_null() || size == 0 || out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }
        let bytes = unsafe { std::slice::from_raw_parts(bytes, size) };

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let result = state
                .runtime
                .restore_document_history_preserving_editor_styles(bytes)
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_undo(runtime: SnowRuntime) -> SnowError {
    ffi_error(|| {
        let mut changed_viewports = std::ptr::null_mut();
        let error = unsafe { snow_runtime_undo_ex(runtime, &mut changed_viewports) };
        unsafe {
            snow_changed_viewports_destroy(changed_viewports);
        }
        error
    })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_undo_ex(
    runtime: SnowRuntime,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let result = state
                .runtime
                .undo_with_viewport_changes()
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `engine` is non-null, it must be a live handle returned by `snow_engine_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_engine_undo(engine: SnowEngine) -> SnowError {
    ffi_error(|| unsafe { snow_runtime_undo(engine) })
}

/// # Safety
/// If `engine` is non-null, it must be a live handle returned by `snow_engine_create`.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_engine_undo_ex(
    engine: SnowEngine,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| unsafe { snow_runtime_undo_ex(engine, out_changed_viewports) })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_redo(runtime: SnowRuntime) -> SnowError {
    ffi_error(|| {
        let mut changed_viewports = std::ptr::null_mut();
        let error = unsafe { snow_runtime_redo_ex(runtime, &mut changed_viewports) };
        unsafe {
            snow_changed_viewports_destroy(changed_viewports);
        }
        error
    })
}

/// # Safety
/// If `runtime` is non-null, it must be a live handle returned by `snow_runtime_create`.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_runtime_redo_ex(
    runtime: SnowRuntime,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| {
        if out_changed_viewports.is_null() {
            return SnowError::InvalidArgument;
        }

        ffi_status(with_runtime_impl_mut(runtime, |state| {
            let result = state
                .runtime
                .redo_with_viewport_changes()
                .map_err(SnowError::from)?;
            write_changed_viewports(out_changed_viewports, result.changed_viewports);
            Ok(())
        }))
    })
}

/// # Safety
/// If `engine` is non-null, it must be a live handle returned by `snow_engine_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_engine_redo(engine: SnowEngine) -> SnowError {
    ffi_error(|| unsafe { snow_runtime_redo(engine) })
}

/// # Safety
/// If `engine` is non-null, it must be a live handle returned by `snow_engine_create`.
/// `out_changed_viewports` must be valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn snow_engine_redo_ex(
    engine: SnowEngine,
    out_changed_viewports: *mut SnowChangedViewportList,
) -> SnowError {
    ffi_error(|| unsafe { snow_runtime_redo_ex(engine, out_changed_viewports) })
}
