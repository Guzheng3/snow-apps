import { invoke } from '@tauri-apps/api/core';
import { ElementRect, ImageBuffer, ImageEncoder } from '.';

export const switchAlwaysOnTop = async (windowId: number) => {
    const result = await invoke<string>('switch_always_on_top', {
        windowId,
    });
    return result;
};

export const setDrawWindowStyle = async () => {
    const result = await invoke('set_draw_window_style');
    return result;
};

/**
 * 捕获焦点窗口
 * @param filePath 文件路径
 * @param copyToClipboard 是否复制到剪贴板
 */
export const captureFocusedWindow = async (
    filePath: string,
    copyToClipboard: boolean,
    focusWindowAppNameVariableName: string,
) => {
    const result = await invoke('capture_focused_window', {
        filePath,
        copyToClipboard,
        focusWindowAppNameVariableName,
    });
    return result;
};

export const captureAllMonitors = async (
    enableMultipleMonitor: boolean,
): Promise<ImageBuffer | undefined> => {
    const result = await invoke<ArrayBuffer>('capture_all_monitors', {
        enableMultipleMonitor,
    });

    if (result.byteLength === 0) {
        return undefined;
    }

    return {
        encoder: ImageEncoder.Png,
        data: new Blob([result]),
        buffer: result,
    };
};

export type CaptureFullScreenResult = {
    monitor_rect: ElementRect;
};

export const captureFullScreen = async (
    enableMultipleMonitor: boolean,
    filePath: string,
    copyToClipboard: boolean,
    captureHistoryFilePath: string,
): Promise<CaptureFullScreenResult> => {
    const result = await invoke<CaptureFullScreenResult>('capture_full_screen', {
        enableMultipleMonitor,
        filePath,
        copyToClipboard,
        captureHistoryFilePath,
    });
    return result;
};
