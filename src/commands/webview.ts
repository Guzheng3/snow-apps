import { invoke } from '@tauri-apps/api/core';

export const createWebViewSharedBuffer = async (data: ArrayBuffer) => {
    const result = await invoke<void>('create_webview_shared_buffer', { data });
    return result;
};

export const setSupportWebViewSharedBuffer = async (value: boolean) => {
    const result = await invoke<void>('set_support_webview_shared_buffer', { value });
    return result;
};
