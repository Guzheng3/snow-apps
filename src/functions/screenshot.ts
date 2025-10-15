import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { captureFocusedWindow } from '@/commands/screenshot';
import { getImagePathFromSettings } from '@/utils/file';
import { playCameraShutterSound } from '@/utils/audio';
import { emit } from '@tauri-apps/api/event';
import * as tauriLog from '@tauri-apps/plugin-log';
import { ScreenshotType } from '@/utils/types';
import { FOCUS_WINDOW_APP_NAME_ENV_VARIABLE } from '@/constants/components/chat';

export const executeScreenshot = async (
    type: ScreenshotType = ScreenshotType.Default,
    windowLabel?: string,
    captureHistoryId?: string,
) => {
    await emit('execute-screenshot', {
        type,
        windowLabel,
        captureHistoryId,
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
        FOCUS_WINDOW_APP_NAME_ENV_VARIABLE,
    );
    // 播放相机快门音效
    playCameraShutterSound();
};

export const finishScreenshot = async () => {
    await emit('finish-screenshot');
};

export const releaseDrawPage = async (force: boolean = false) => {
    await emit('release-draw-page', {
        force,
    });
};

export const onCaptureHistoryChange = async () => {
    await emit('on-capture-history-change');
};
