import { getPlatform } from '@/utils';

export type ImageSharedBufferData = {
    sharedBuffer: Uint8ClampedArray<ArrayBuffer>;
    width: number;
    height: number;
};

export const getImageBufferFromSharedBuffer = (): Promise<ImageSharedBufferData | undefined> => {
    if (getPlatform() !== 'windows' || !('chrome' in window)) {
        return Promise.resolve(undefined);
    }

    // Windows 下支持通过 SharedBuffer 传输图像数据
    return new Promise((resolve) => {
        const chromeWindows = window as unknown as {
            chrome: {
                webview: {
                    addEventListener: (
                        event: string,
                        callback: (e: { getBuffer: () => ArrayBuffer }) => void,
                    ) => void;
                    removeEventListener: (
                        event: string,
                        callback: (e: { getBuffer: () => ArrayBuffer }) => void,
                    ) => void;
                    releaseBuffer: (buffer: ArrayBuffer) => void;
                };
            };
        };

        const handleSharedBufferReceived = (e: { getBuffer: () => ArrayBuffer }) => {
            clearTimeout(timeout);

            const buffer = e.getBuffer();
            const imageExtraInfoBytesLength = 8;
            const imageBytesLength = buffer.byteLength - imageExtraInfoBytesLength;
            const width = new DataView(buffer, imageBytesLength, 4).getUint32(0, true);
            const height = new DataView(buffer, imageBytesLength + 4, 4).getUint32(0, true);

            resolve({
                sharedBuffer: new Uint8ClampedArray(buffer.slice(0, imageBytesLength)),
                width,
                height,
            });
            chromeWindows.chrome.webview.removeEventListener(
                'sharedbufferreceived',
                handleSharedBufferReceived,
            );
            // 释放 SharedBuffer
            chromeWindows.chrome.webview.releaseBuffer(buffer);
        };

        chromeWindows.chrome.webview.addEventListener(
            'sharedbufferreceived',
            handleSharedBufferReceived,
        );

        const timeout = setTimeout(() => {
            resolve(undefined);
            chromeWindows.chrome.webview.removeEventListener(
                'sharedbufferreceived',
                handleSharedBufferReceived,
            );
        }, 1000 * 5);
    });
};
