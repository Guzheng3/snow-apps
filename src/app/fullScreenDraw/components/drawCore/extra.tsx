import {
    AppState,
    BinaryFiles,
    ExcalidrawActionType,
    PointerDownState,
    ToolType,
} from '@mg-chao/excalidraw/types';
import { ExcalidrawImperativeAPI } from '@mg-chao/excalidraw/types';
import { createPublisher } from '@/hooks/useStatePublisher';
import { ExcalidrawElement, OrderedExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { ElementRect } from '@/types/commands/screenshot';
import { createContext } from 'react';
import { MousePosition } from '@/utils/mousePosition';
import { DragElementOptionalConfig } from '@/app/draw/components/drawToolbar/components/dragButton';
import { DrawState } from '@/types/draw';

export type DrawCoreActionType = {
    setActiveTool: (
        tool: (
            | {
                  type: ToolType;
              }
            | {
                  type: 'custom';
                  customType: string;
              }
        ) & {
            locked?: boolean;
            fromSelection?: boolean;
        },
        keepSelection?: boolean,
        drawState?: DrawState,
    ) => void;
    syncActionResult: ExcalidrawActionType['syncActionResult'];
    updateScene: ExcalidrawImperativeAPI['updateScene'];
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

/**
 * Watermark 文本更新时发生
 */
export type ExcalidrawEventOnWatermarkTextChangeParams = {
    event: 'onWatermarkTextChange';
    params: {
        text: string;
    };
};

export type ExcalidrawEventParams =
    | ExcalidrawEventOnChangeParams
    | ExcalidrawEventOnPointerDownParams
    | ExcalidrawEventOnPointerUpParams
    | ExcalidrawEventOnDrawParams
    | ExcalidrawEventOnWatermarkTextChangeParams;

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
    drawState?: DrawState;
};

export type ExcalidrawEventCallbackParams = {
    event: ExcalidrawEventCallbackType.ChangeFontSize;
    params: ExcalidrawEventCallbackFontSizeParams;
};

export const ExcalidrawEventCallbackPublisher = createPublisher<
    ExcalidrawEventCallbackParams | undefined
>(undefined, true);

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
    getDragElementOptionalConfig?: (
        limitRect: ElementRect,
        devicePixelRatio: number,
    ) => DragElementOptionalConfig[];
    getAction: () => DrawCoreActionType | undefined;
    getMousePosition: () => MousePosition | undefined;
    calculatedBoundaryRect?: (
        rect: ElementRect,
        toolbarWidth: number,
        toolbarHeight: number,
        viewportWidth: number,
        viewportHeight: number,
    ) => ElementRect;
    getContentScale?: () => number;
    getPopoverPopupContainer?: () => HTMLElement;
};

export const DrawCoreContext = createContext<DrawCoreContextValue>({
    getLimitRect: () => undefined,
    getDevicePixelRatio: () => window.devicePixelRatio,
    getBaseOffset: () => ({
        x: 0,
        y: 0,
    }),
    getDragElementOptionalConfig: undefined,
    getAction: () => undefined,
    getMousePosition: () => undefined,
});
