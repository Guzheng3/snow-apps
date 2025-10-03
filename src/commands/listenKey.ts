import { getPlatform } from '@/utils';
import { invoke } from '@tauri-apps/api/core';

export type ListenKeyDownEvent = {
    key: number;
};

export type ListenKeyUpEvent = {
    key: number;
};

export enum ListenKeyCode {
    LControl = 58,
    RControl = 59,
    LShift = 60,
    RShift = 61,
    LAlt = 62,
    RAlt = 63,
    Command = 64,
    RCommand = 65,
    LOption = 66,
    ROption = 67,
    LMeta = 68,
    RMeta = 69,
}

export const LISTEN_KEY_SERVICE_KEY_DOWN_EMIT_KEY = 'listen-key-service:key-down';
export const LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY = 'listen-key-service:key-up';
export const LISTEN_KEY_SERVICE_STOP_EMIT_KEY = 'listen-key-service:stop'; // 停止监听键盘

export const LISTEN_KEY_SERVICE_MOUSE_DOWN_EMIT_KEY = 'listen-mouse-service:mouse-down';
export const LISTEN_KEY_SERVICE_MOUSE_UP_EMIT_KEY = 'listen-mouse-service:mouse-up';
export const LISTEN_KEY_SERVICE_MOUSE_STOP_EMIT_KEY = 'listen-mouse-service:mouse-stop'; // 停止监听鼠标

export const listenKeyStart = async () => {
    // macOS 下 Ctrl、Shift、Command 等键浏览器不会响应，特殊处理下
    if (getPlatform() !== 'macos') {
        return;
    }

    const result = await invoke<void>('listen_key_start');
    return result;
};

export const listenKeyStop = async () => {
    if (getPlatform() !== 'macos') {
        return;
    }

    const result = await invoke<void>('listen_key_stop');
    return result;
};

export const listenKeyStopByWindowLabel = async (windowLabel: string) => {
    if (getPlatform() !== 'macos') {
        return;
    }

    const result = await invoke<void>('listen_key_stop_by_window_label', { windowLabel });
    return result;
};

export const listenMouseStart = async () => {
    const result = await invoke<void>('listen_mouse_start');
    return result;
};

export const listenMouseStop = async () => {
    const result = await invoke<void>('listen_mouse_stop');
    return result;
};

export const listenMouseStopByWindowLabel = async (windowLabel: string) => {
    if (getPlatform() !== 'macos') {
        return;
    }

    const result = await invoke<void>('listen_mouse_stop_by_window_label', { windowLabel });
    return result;
};
