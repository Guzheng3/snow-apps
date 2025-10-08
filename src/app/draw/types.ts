import React from 'react';
import { DrawLayerActionType } from './components/drawLayer';
import { SelectLayerActionType } from './components/selectLayer';
import { ImageBuffer } from '@/commands';
import { DrawToolbarActionType } from './components/drawToolbar';
import { MousePosition } from '@/utils/mousePosition';
import { DrawCacheLayerActionType } from './components/drawCacheLayer/extra';
import { OcrBlocksActionType } from './components/ocrBlocks';
import { ColorPickerActionType } from './components/colorPicker';
import { CaptureBoundingBoxInfo } from './extra';
import { CaptureHistoryActionType } from './components/captureHistory';
import { ImageSharedBufferData } from './tools';

export enum CaptureStep {
    // 选择阶段
    Select = 1,
    // 绘制阶段
    Draw = 2,
    // 固定阶段
    Fixed = 3,
}

export enum CanvasLayer {
    Draw = 1,
    Select = 2,
}

export type DrawContextType = {
    finishCapture: (clearScrollScreenshot?: boolean) => Promise<void>;
    drawLayerActionRef: React.RefObject<DrawLayerActionType | undefined>;
    selectLayerActionRef: React.RefObject<SelectLayerActionType | undefined>;
    imageBufferRef: React.RefObject<ImageBuffer | ImageSharedBufferData | undefined>;
    mousePositionRef: React.RefObject<MousePosition>;
    drawToolbarActionRef: React.RefObject<DrawToolbarActionType | undefined>;
    circleCursorRef: React.RefObject<HTMLDivElement | null>;
    drawCacheLayerActionRef: React.RefObject<DrawCacheLayerActionType | undefined>;
    ocrBlocksActionRef: React.RefObject<OcrBlocksActionType | undefined>;
    colorPickerActionRef: React.RefObject<ColorPickerActionType | undefined>;
    captureBoundingBoxInfoRef: React.RefObject<CaptureBoundingBoxInfo | undefined>;
    captureHistoryActionRef: React.RefObject<CaptureHistoryActionType | undefined>;
};

export const DrawContext = React.createContext<DrawContextType>({
    mousePositionRef: { current: new MousePosition(0, 0) },
    imageBufferRef: { current: undefined },
    finishCapture: () => Promise.resolve(),
    drawLayerActionRef: { current: undefined },
    selectLayerActionRef: { current: undefined },
    drawToolbarActionRef: { current: undefined },
    circleCursorRef: { current: null },
    drawCacheLayerActionRef: { current: undefined },
    ocrBlocksActionRef: { current: undefined },
    colorPickerActionRef: { current: undefined },
    captureBoundingBoxInfoRef: { current: undefined },
    captureHistoryActionRef: { current: undefined },
});
