import * as clipboard from '@tauri-apps/plugin-clipboard-manager';
import { appWarn } from './log';

export const writeTextToClipboard = async (text: string) => {
    let isSuccess = false;
    try {
        await clipboard.writeText(text);
        isSuccess = true;
    } catch (error) {
        isSuccess = false;
        appWarn('[clipboard] writeTextToClipboard error', error);
    }

    if (isSuccess) {
        return;
    }

    await navigator.clipboard.write([new ClipboardItem({ 'text/plain': text })]);
};

export const writeImageToClipboard = async (image: Blob | ArrayBuffer, format = 'image/png') => {
    let isSuccess = false;
    try {
        await clipboard.writeImage(image instanceof Blob ? await image.arrayBuffer() : image);
        isSuccess = true;
    } catch (error) {
        appWarn('[clipboard] writeImageToClipboard error', error);
    }

    if (isSuccess) {
        return;
    }

    await navigator.clipboard.write([
        new ClipboardItem({ [format]: image instanceof Blob ? image : new Blob([image]) }),
    ]);
};

export const writeHtmlToClipboard = async (html: string) => {
    let isSuccess = false;
    try {
        await clipboard.writeHtml(html);
        isSuccess = true;
    } catch (error) {
        isSuccess = false;
        appWarn('[clipboard] writeHtmlToClipboard error', error);
    }

    if (isSuccess) {
        return;
    }
    await navigator.clipboard.write([new ClipboardItem({ 'text/html': html })]);
};
