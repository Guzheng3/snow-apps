import { OcrModel } from '@/types/appSettings';
import { OcrDetectResult } from '@/types/commands/ocr';
import { invoke } from '@tauri-apps/api/core';

export const ocrDetect = async (
    data: ArrayBuffer | Uint8Array,
    scaleFactor: number,
    detectAngle: boolean,
): Promise<OcrDetectResult> => {
    return await invoke<OcrDetectResult>('ocr_detect', data, {
        headers: {
            'x-scale-factor': scaleFactor.toFixed(3),
            'x-detect-angle': detectAngle ? 'true' : 'false',
        },
    });
};

export const ocrInit = async (
    orcPluginPath: string,
    model: OcrModel,
    hotStart: boolean,
    modelWriteToMemory: boolean,
): Promise<void> => {
    await invoke<void>('ocr_init', { orcPluginPath, model, hotStart, modelWriteToMemory });
};

export const ocrRelease = async (): Promise<void> => {
    await invoke<void>('ocr_release');
};
