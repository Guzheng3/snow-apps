import { invoke } from '@tauri-apps/api/core';

export interface OcrDetectResultTextPoint {
    x: number;
    y: number;
}

export interface OcrDetectResultTextBlock {
    box_points: OcrDetectResultTextPoint[];
    text: string;
    text_score: number;
}

export interface OcrDetectResult {
    text_blocks: OcrDetectResultTextBlock[];
    scale_factor: number;
}

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

export enum OcrModel {
    RapidOcrV4 = 'RapidOcrV4',
    RapidOcrV5 = 'RapidOcrV5',
}

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
