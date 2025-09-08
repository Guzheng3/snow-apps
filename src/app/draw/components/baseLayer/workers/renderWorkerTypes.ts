import { ElementRect } from '@/commands';
import { ApplicationOptions } from 'pixi.js';
import { BlurSpriteProps, WatermarkProps } from '../baseLayerRenderActions';
import * as PIXI from 'pixi.js';

export type RefWrap<T> = {
    current: T;
};

export enum BaseLayerRenderMessageType {
    Init = 'init',
    Dispose = 'dispose',
    CreateNewCanvasContainer = 'createNewCanvasContainer',
    ResizeCanvas = 'resizeCanvas',
    ClearCanvas = 'clearCanvas',
    GetImageData = 'getImageData',
    RenderToCanvas = 'renderToCanvas',
    CanvasRender = 'canvasRender',
    AddImageToContainer = 'addImageToContainer',
    ClearContainer = 'clearContainer',
    CreateBlurSprite = 'createBlurSprite',
    UpdateBlurSprite = 'updateBlurSprite',
    UpdateWatermarkSprite = 'updateWatermarkSprite',
    DeleteBlurSprite = 'deleteBlurSprite',
}

export type BaseLayerRenderInitData = {
    type: BaseLayerRenderMessageType.Init;
    payload: {
        appOptions: Partial<ApplicationOptions>;
    };
};

export type BaseLayerRenderDisposeData = {
    type: BaseLayerRenderMessageType.Dispose;
};

export type BaseLayerRenderCreateNewCanvasContainerData = {
    type: BaseLayerRenderMessageType.CreateNewCanvasContainer;
    payload: {
        containerKey: string;
    };
};

export type BaseLayerRenderResizeCanvasData = {
    type: BaseLayerRenderMessageType.ResizeCanvas;
    payload: {
        width: number;
        height: number;
    };
};

export type BaseLayerRenderClearCanvasData = {
    type: BaseLayerRenderMessageType.ClearCanvas;
};

export type BaseLayerRenderGetImageDataData = {
    type: BaseLayerRenderMessageType.GetImageData;
    payload: {
        selectRect: ElementRect;
    };
};

export type BaseLayerRenderRenderToCanvasData = {
    type: BaseLayerRenderMessageType.RenderToCanvas;
    payload: {
        selectRect: ElementRect;
    };
};

export type BaseLayerRenderCanvasRenderData = {
    type: BaseLayerRenderMessageType.CanvasRender;
};

export type BaseLayerRenderAddImageToContainerData = {
    type: BaseLayerRenderMessageType.AddImageToContainer;
    payload: {
        containerKey: string;
        imageSrc: string;
    };
};

export type BaseLayerRenderClearContainerData = {
    type: BaseLayerRenderMessageType.ClearContainer;
    payload: {
        containerKey: string;
    };
};

export type BaseLayerRenderCreateBlurSpriteData = {
    type: BaseLayerRenderMessageType.CreateBlurSprite;
    payload: {
        blurContainerKey: string;
        blurElementId: string;
    };
};

export type BaseLayerRenderUpdateBlurSpriteData = {
    type: BaseLayerRenderMessageType.UpdateBlurSprite;
    payload: {
        blurElementId: string;
        blurProps: BlurSpriteProps;
        updateFilter: boolean;
    };
};

export type BaseLayerRenderUpdateWatermarkSpriteData = {
    type: BaseLayerRenderMessageType.UpdateWatermarkSprite;
    payload: {
        watermarkContainerKey: string;
        watermarkProps: WatermarkProps;
        textResolution: number;
    };
};

export type BaseLayerRenderDeleteBlurSpriteData = {
    type: BaseLayerRenderMessageType.DeleteBlurSprite;
    payload: {
        blurElementId: string;
    };
};

export type BaseLayerRenderData =
    | BaseLayerRenderInitData
    | BaseLayerRenderDisposeData
    | BaseLayerRenderCreateNewCanvasContainerData
    | BaseLayerRenderResizeCanvasData
    | BaseLayerRenderClearCanvasData
    | BaseLayerRenderGetImageDataData
    | BaseLayerRenderRenderToCanvasData
    | BaseLayerRenderCanvasRenderData
    | BaseLayerRenderAddImageToContainerData
    | BaseLayerRenderClearContainerData
    | BaseLayerRenderCreateBlurSpriteData
    | BaseLayerRenderUpdateBlurSpriteData
    | BaseLayerRenderUpdateWatermarkSpriteData
    | BaseLayerRenderDeleteBlurSpriteData;

export type RenderInitResult = {
    type: BaseLayerRenderMessageType.Init;
    payload: OffscreenCanvas | HTMLCanvasElement | undefined;
};

export type RenderDisposeResult = {
    type: BaseLayerRenderMessageType.Dispose;
    payload: undefined;
};

export type RenderCreateNewCanvasContainerResult = {
    type: BaseLayerRenderMessageType.CreateNewCanvasContainer;
    payload: {
        containerKey: string | undefined;
    };
};

export type RenderResizeCanvasResult = {
    type: BaseLayerRenderMessageType.ResizeCanvas;
    payload: undefined;
};

export type RenderClearCanvasResult = {
    type: BaseLayerRenderMessageType.ClearCanvas;
    payload: undefined;
};

export type RenderGetImageDataResult = {
    type: BaseLayerRenderMessageType.GetImageData;
    payload: {
        imageData: ImageData | undefined;
    };
};

export type RenderRenderToCanvasResult = {
    type: BaseLayerRenderMessageType.RenderToCanvas;
    payload: {
        canvas: PIXI.ICanvas | undefined;
    };
};

export type RenderCanvasRenderResult = {
    type: BaseLayerRenderMessageType.CanvasRender;
    payload: undefined;
};

export type RenderAddImageToContainerResult = {
    type: BaseLayerRenderMessageType.AddImageToContainer;
    payload: undefined;
};

export type RenderClearContainerResult = {
    type: BaseLayerRenderMessageType.ClearContainer;
    payload: undefined;
};

export type RenderCreateBlurSpriteResult = {
    type: BaseLayerRenderMessageType.CreateBlurSprite;
    payload: undefined;
};

export type RenderUpdateBlurSpriteResult = {
    type: BaseLayerRenderMessageType.UpdateBlurSprite;
    payload: undefined;
};

export type RenderUpdateWatermarkSpriteResult = {
    type: BaseLayerRenderMessageType.UpdateWatermarkSprite;
    payload: undefined;
};

export type RenderDeleteBlurSpriteResult = {
    type: BaseLayerRenderMessageType.DeleteBlurSprite;
    payload: undefined;
};

export type RenderBlurSpriteResult =
    | RenderCreateBlurSpriteResult
    | RenderUpdateBlurSpriteResult
    | RenderUpdateWatermarkSpriteResult
    | RenderDeleteBlurSpriteResult;

export type RenderResult =
    | RenderInitResult
    | RenderDisposeResult
    | RenderCreateNewCanvasContainerResult
    | RenderResizeCanvasResult
    | RenderClearCanvasResult
    | RenderGetImageDataResult
    | RenderRenderToCanvasResult
    | RenderCanvasRenderResult
    | RenderAddImageToContainerResult
    | RenderClearContainerResult
    | RenderBlurSpriteResult
    | RenderClearContainerResult
    | RenderUpdateWatermarkSpriteResult;
