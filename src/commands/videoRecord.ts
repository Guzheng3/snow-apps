import { invoke } from '@tauri-apps/api/core';

export enum VideoFormat {
    Mp4 = 'Mp4',
    Gif = 'Gif',
}

export enum VideoMaxSize {
    P2160 = '2160p',
    P1440 = '1440p',
    P1080 = '1080p',
    P720 = '720p',
    P480 = '480p',
}

export enum GifFormat {
    Gif = 'gif',
    Apng = 'apng',
    Webp = 'webp',
}

export const videoRecordStart = async (
    minX: number,
    minY: number,
    maxX: number,
    maxY: number,
    outputFile: string,
    format: VideoFormat,
    frameRate: number,
    enableMicrophone: boolean,
    enableSystemAudio: boolean,
    microphoneDeviceName: string,
    hwaccel: boolean,
    encoder: string,
    encoderPreset: string,
    videoMaxWidth: number,
    videoMaxHeight: number,
) => {
    const result = await invoke('video_record_start', {
        minX,
        minY,
        maxX,
        maxY,
        outputFile,
        format,
        frameRate,
        enableMicrophone,
        enableSystemAudio,
        microphoneDeviceName,
        hwaccel,
        encoder,
        encoderPreset,
        videoMaxWidth,
        videoMaxHeight,
    });
    return result;
};

export const videoRecordStop = async (
    convertToGif: boolean,
    gifFormat: GifFormat,
    gifFrameRate: number,
    gifMaxWidth: number,
    gifMaxHeight: number,
): Promise<string | null | undefined> => {
    const result = await invoke<string | null | undefined>('video_record_stop', {
        convertToGif,
        gifFormat,
        gifFrameRate,
        gifMaxWidth,
        gifMaxHeight,
    });
    return result;
};

export const videoRecordPause = async () => {
    const result = await invoke('video_record_pause');
    return result;
};

export const videoRecordResume = async () => {
    const result = await invoke('video_record_resume');
    return result;
};

export const videoRecordKill = async () => {
    const result = await invoke('video_record_kill');
    return result;
};

export const videoRecordGetMicrophoneDeviceNames = async () => {
    const result = await invoke<string[]>('video_record_get_microphone_device_names');
    return result;
};

export const videoRecordInit = async (ffmpegPluginDir: string) => {
    const result = await invoke('video_record_init', { ffmpegPluginDir });
    return result;
};
