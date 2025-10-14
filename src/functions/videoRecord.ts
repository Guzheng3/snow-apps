import { VideoRecordState } from '@/app/videoRecord/extra';
import { hasVideoRecordWindow } from '@/commands/core';
import { emit } from '@tauri-apps/api/event';
import { executeScreenshot } from './screenshot';
import { ScreenshotType } from '@/utils/types';

export const changeVideoRecordState = async (state: VideoRecordState) => {
    await emit('change-video-record-state', {
        state,
    });
};

export const startOrCopyVideo = async () => {
    if (await hasVideoRecordWindow()) {
        await emit('start-or-copy-video');
    } else {
        executeScreenshot(ScreenshotType.VideoRecord);
    }
};
