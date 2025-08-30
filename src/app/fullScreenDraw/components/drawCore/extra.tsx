import {
    AppState,
    BinaryFiles,
    ExcalidrawActionType,
    PointerDownState,
} from '@mg-chao/excalidraw/types';
import { ExcalidrawImperativeAPI } from '@mg-chao/excalidraw/types';
import { createPublisher } from '@/hooks/useStatePublisher';
import { ExcalidrawElement, OrderedExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { ElementRect } from '@/commands';
import { createContext } from 'react';
import { MousePosition } from '@/utils/mousePosition';

export type DrawCoreActionType = {
    setActiveTool: ExcalidrawImperativeAPI['setActiveTool'];
    syncActionResult: ExcalidrawActionType['syncActionResult'];
    updateScene: ExcalidrawImperativeAPI['updateScene'];
    setEnable: (enable: boolean) => void;
    getImageData: (selectRect: ElementRect) => Promise<ImageData | undefined>;
    getCanvasContext: () => CanvasRenderingContext2D | null | undefined;
    getCanvas: () => HTMLCanvasElement | null;
    getAppState: () => AppState | undefined;
    getDrawCacheLayerElement: () => HTMLDivElement | null;
    getExcalidrawAPI: () => ExcalidrawImperativeAPI | undefined;
    finishDraw: () => void;
};

export type ExcalidrawKeyEvent = {
    resizeFromCenter: boolean;
    maintainAspectRatio: boolean;
    rotateWithDiscreteAngle: boolean;
    autoAlign: boolean;
};

export const ExcalidrawKeyEventPublisher = createPublisher<ExcalidrawKeyEvent>({
    resizeFromCenter: false,
    maintainAspectRatio: false,
    rotateWithDiscreteAngle: false,
    autoAlign: false,
});

export const convertLocalToLocalCode = (local: string) => {
    switch (local) {
        case 'zh-Hans':
            return 'zh-CN';
        case 'zh-Hant':
            return 'zh-TW';
        case 'en':
            return 'en-US';
        default:
            return local;
    }
};

export type ExcalidrawEventOnChangeParams = {
    event: 'onChange';
    params: {
        elements: readonly OrderedExcalidrawElement[];
        appState: AppState;
        files: BinaryFiles;
    };
};

export type ExcalidrawEventOnPointerDownParams = {
    event: 'onPointerDown';
    params: {
        activeTool: AppState['activeTool'];
        pointerDownState: PointerDownState;
    };
};

export type ExcalidrawEventOnPointerUpParams = {
    event: 'onPointerUp';
    params: {
        activeTool: AppState['activeTool'];
        pointerDownState: PointerDownState;
    };
};

/**
 * 开始新一次绘制时发送
 */
export type ExcalidrawEventOnDrawParams = {
    event: 'onDraw';
    params: undefined;
};

export type ExcalidrawEventParams =
    | ExcalidrawEventOnChangeParams
    | ExcalidrawEventOnPointerDownParams
    | ExcalidrawEventOnPointerUpParams
    | ExcalidrawEventOnDrawParams;

export const ExcalidrawEventPublisher = createPublisher<ExcalidrawEventParams | undefined>(
    undefined,
    true,
);

export type ExcalidrawOnHandleEraserParams = {
    elements: Set<ExcalidrawElement['id']>;
};

export const ExcalidrawOnHandleEraserPublisher = createPublisher<
    ExcalidrawOnHandleEraserParams | undefined
>(undefined);

export enum ExcalidrawEventCallbackType {
    ChangeFontSize = 'ChangeFontSize',
}

export type ExcalidrawEventCallbackFontSizeParams = {
    fontSize: number;
};

export type ExcalidrawEventCallbackParams = {
    event: ExcalidrawEventCallbackType.ChangeFontSize;
    params: ExcalidrawEventCallbackFontSizeParams;
};

export const ExcalidrawEventCallbackPublisher = createPublisher<
    ExcalidrawEventCallbackParams | undefined
>(undefined, true);

export enum DrawState {
    Idle = 0,
    // 选择元素
    Select = 1,
    // 矩形
    Rect = 2,
    // 菱形
    Diamond = 3,
    // 椭圆
    Ellipse = 4,
    // 箭头
    Arrow = 5,
    // 线条
    Line = 6,
    // 画笔
    Pen = 7,
    // 文本
    Text = 8,
    // 序列号
    SerialNumber = 9,
    // 模糊
    Blur = 10,
    // 橡皮擦
    Eraser = 11,
    // 锁定
    Lock = 12,
    // 撤销
    Undo = 101,
    // 重做
    Redo = 102,
    // 取消
    Cancel = 103,
    // 保存
    Save = 104,
    // 快速保存
    FastSave = 105,
    // 固定
    Fixed = 106,
    // 复制
    Copy = 107,
    // OCR
    OcrDetect = 108,
    // OCR 翻译
    OcrTranslate = 108001,
    // 滚动截图
    ScrollScreenshot = 109,
    // 额外工具
    ExtraTools = 110,
    // 扫描二维码
    ScanQrcode = 111,
    // 激光笔
    LaserPointer = 112,
    // 鼠标穿透
    MouseThrough = 113,
    // 视频录制
    VideoRecord = 114,
}

export const DrawStatePublisher = createPublisher<DrawState>(DrawState.Idle);

export type DrawCoreContextValue = {
    getLimitRect: () => ElementRect | undefined;
    getDevicePixelRatio: () => number;
    getBaseOffset: (
        limitRect: ElementRect,
        devicePixelRatio: number,
    ) => {
        x: number;
        y: number;
    };
    getAction: () => DrawCoreActionType | undefined;
    getMousePosition: () => MousePosition | undefined;
};

export const DrawCoreContext = createContext<DrawCoreContextValue>({
    getLimitRect: () => undefined,
    getDevicePixelRatio: () => window.devicePixelRatio,
    getBaseOffset: () => ({
        x: 0,
        y: 0,
    }),
    getAction: () => undefined,
    getMousePosition: () => undefined,
});
