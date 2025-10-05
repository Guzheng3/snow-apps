import { VideoRecordState } from '@/app/videoRecord/extra';
import { hasVideoRecordWindow } from '@/commands/core';
import { emit } from '@tauri-apps/api/event';
import { executeScreenshot, ScreenshotType } from './screenshot';

export type VideoRecordWindowInfo = {
    select_rect_max_x: number;
    select_rect_max_y: number;
    select_rect_min_x: number;
    select_rect_min_y: number;
};

export const closeVideoRecordWindow = async () => {
    await emit('close-video-record-window');
};

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
