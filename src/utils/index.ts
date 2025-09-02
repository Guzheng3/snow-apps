import * as tauriOs from '@tauri-apps/plugin-os';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { Base64 } from 'js-base64';
import { writeTextToClipboard } from './clipboard';

export const encodeParamsValue = (value: string) => {
    return encodeURIComponent(Base64.encode(value));
};

export const decodeParamsValue = (value: string) => {
    return Base64.decode(decodeURIComponent(value));
};

export const copyText = (text: string) => {
    const selected = window.getSelection();
    if (selected && selected.toString().trim()) {
        writeTextToClipboard(selected.toString());
        selected.removeAllRanges();
    } else {
        writeTextToClipboard(text);
    }
};

export const copyTextAndHide = (text: string) => {
    copyText(text);
    getCurrentWindow().hide();
};

let platformCache: tauriOs.Platform | undefined = undefined;
/**
 * nextjs 会在服务端执行一次 js 代码，所以直接调用 tauriOs.platform() 会报错
 * 但项目都使用了 use client，最终都会在浏览器环境执行，这里提供默认的值来取消报错
 */
export const getPlatform = () => {
    if (platformCache) {
        return platformCache;
    }

    let platform: tauriOs.Platform = 'windows';
    if (typeof window !== 'undefined') {
        platform = tauriOs.platform();
    }

    platformCache = platform;

    return platform;
};

/**
 * 根据平台获取某个参数值
 * @param defaultValue 默认值（windows 平台值）
 * @param macosValue macos 平台值
 * @param linuxValue linux 平台值
 * @returns 平台值
 */
export const getPlatformValue = <T>(defaultValue: T, macosValue?: T, linuxValue?: T): T => {
    let value: T | undefined = defaultValue;

    switch (getPlatform()) {
        case 'macos':
            value = macosValue;
            break;
        case 'linux':
            value = linuxValue;
            break;
        default:
            break;
    }

    return value ?? defaultValue;
};

/**
 * 判断是否支持 OffscreenCanvas
 * @returns 是否支持
 */
export const supportOffscreenCanvas = () => {
    if (typeof window === 'undefined') {
        return false;
    }

    return 'OffscreenCanvas' in window;
};

const RANDOM_STRING_CHARS = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
export const randomString = (length: number) => {
    let result = '';
    for (let i = length; i > 0; --i) {
        result += RANDOM_STRING_CHARS[Math.floor(Math.random() * RANDOM_STRING_CHARS.length)];
    }
    return result;
};
