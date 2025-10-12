'use client';

import { getPlatform } from '.';

export const getWebViewSharedBuffer = (): Promise<ArrayBuffer | undefined> => {
    if (getPlatform() !== 'windows' || !('chrome' in window)) {
        return Promise.resolve(undefined);
    }

    // Windows 下支持通过 SharedBuffer 传输图像数据
    return new Promise((resolve) => {
        const handleSharedBufferReceived = (e: { getBuffer: () => ArrayBuffer }) => {
            clearTimeout(timeout);

            const buffer = e.getBuffer();

            resolve(buffer);
            window.chrome.webview.removeEventListener(
                'sharedbufferreceived',
                handleSharedBufferReceived,
            );
        };

        window.chrome.webview.addEventListener('sharedbufferreceived', handleSharedBufferReceived);

        const timeout = setTimeout(() => {
            resolve(undefined);
            window.chrome.webview.removeEventListener(
                'sharedbufferreceived',
                handleSharedBufferReceived,
            );
        }, 1000 * 3);
    });
};

export const releaseWebViewSharedBuffer = (buffer: ArrayBuffer) => {
    if (getPlatform() !== 'windows' || !('chrome' in window)) {
        return;
    }
    window.chrome.webview.releaseBuffer(buffer);
};
