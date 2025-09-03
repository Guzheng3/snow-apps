import { AppSettingsData, AppSettingsGroup } from '@/app/contextWrap';
import { captureFocusedWindow } from '@/commands/screenshot';
import { getImagePathFromSettings } from '@/utils/file';
import { playSound } from '@/utils/audio';
import { emit } from '@tauri-apps/api/event';
import * as tauriLog from '@tauri-apps/plugin-log';

export enum ScreenshotType {
    Default = 'default',
    Fixed = 'fixed',
    OcrDetect = 'ocr-detect',
    OcrTranslate = 'ocr-translate',
    TopWindow = 'top-window',
    Copy = 'copy',
    VideoRecord = 'video-record',
}

export const executeScreenshot = async (
    type: ScreenshotType = ScreenshotType.Default,
    windowLabel?: string,
) => {
    await emit('execute-screenshot', {
        type,
        windowLabel,
    });
};

export const executeScreenshotFocusedWindow = async (appSettings: AppSettingsData) => {
    const imagePath = await getImagePathFromSettings(appSettings, 'focused-window');
    if (!imagePath) {
        tauriLog.error('[executeScreenshotFocusedWindow] Failed to get image path from settings');

        return;
    }

    captureFocusedWindow(
        imagePath.filePath,
        appSettings[AppSettingsGroup.FunctionScreenshot].focusedWindowCopyToClipboard,
    );
    // 播放相机快门音效
    playSound('/audios/camera_shutter.mp3');
};

export const finishScreenshot = async () => {
    await emit('finish-screenshot');
};

export const releaseDrawPage = async (force: boolean = false) => {
    await emit('release-draw-page', {
        force,
    });
};
