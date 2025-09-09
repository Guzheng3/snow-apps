import { RefType } from '../../baseLayer/baseLayerRenderActions';
import { terminateWorkerAction } from '../actions';
import {
    renderGetPreviewImageDataAction,
    renderInitImageDataAction,
    renderInitPreviewCanvasAction,
    renderPickColorAction,
    renderPutImageDataAction,
    renderSwitchCaptureHistoryAction,
} from '../renderActions';
import {
    ColorPickerRenderData,
    ColorPickerRenderInitImageDataData,
    ColorPickerRenderInitPreviewCanvasData,
    ColorPickerRenderMessageType,
    ColorPickerRenderPickColorData,
    ColorPickerRenderPutImageDataData,
    ColorPickerRenderResult,
    ColorPickerRenderSwitchCaptureHistoryData,
} from './renderWorkerTypes';

const previewCanvasRef: RefType<OffscreenCanvas | null> = {
    current: null,
};
const previewCanvasCtxRef: RefType<OffscreenCanvasRenderingContext2D | null> = {
    current: null,
};
const previewImageDataRef: RefType<ImageData | null> = {
    current: null,
};
const decoderWasmModuleArrayBufferRef: RefType<ArrayBuffer | null> = {
    current: null,
};
const captureHistoryImageDataRef: RefType<ImageData | undefined> = {
    current: undefined,
};

const handleInitPreviewCanvas = async (data: ColorPickerRenderInitPreviewCanvasData) => {
    const { previewCanvas, decoderWasmModuleArrayBuffer } = data.payload;

    previewCanvasRef.current = previewCanvas;
    renderInitPreviewCanvasAction(
        previewCanvasRef,
        previewCanvasCtxRef,
        decoderWasmModuleArrayBufferRef,
        decoderWasmModuleArrayBuffer,
    );
};

const handleInitImageData = async (data: ColorPickerRenderInitImageDataData) => {
    const { imageBuffer } = data.payload;
    await renderInitImageDataAction(
        previewCanvasRef,
        previewImageDataRef,
        decoderWasmModuleArrayBufferRef,
        imageBuffer,
    );
};

const handlePutImageData = async (data: ColorPickerRenderPutImageDataData) => {
    const { x, y, baseIndex, centerAuxiliaryLineColor } = data.payload;
    return renderPutImageDataAction(
        previewCanvasCtxRef,
        previewImageDataRef,
        captureHistoryImageDataRef,
        x,
        y,
        baseIndex,
        centerAuxiliaryLineColor,
    );
};

const handleGetPreviewImageData = async () => {
    return renderGetPreviewImageDataAction(previewImageDataRef, captureHistoryImageDataRef);
};

const handleSwitchCaptureHistory = async (data: ColorPickerRenderSwitchCaptureHistoryData) => {
    const { imageSrc } = data.payload;
    await renderSwitchCaptureHistoryAction(
        decoderWasmModuleArrayBufferRef,
        captureHistoryImageDataRef,
        imageSrc,
    );
};

const handlePickColor = async (data: ColorPickerRenderPickColorData) => {
    const { baseIndex } = data.payload;
    return renderPickColorAction(captureHistoryImageDataRef, previewImageDataRef, baseIndex);
};

self.onmessage = async ({ data }: MessageEvent<ColorPickerRenderData>) => {
    let message: ColorPickerRenderResult;
    switch (data.type) {
        case ColorPickerRenderMessageType.InitPreviewCanvas:
            await handleInitPreviewCanvas(data);
            message = {
                type: ColorPickerRenderMessageType.InitPreviewCanvas,
                payload: undefined,
            };
            break;
        case ColorPickerRenderMessageType.InitImageData:
            await handleInitImageData(data);
            message = {
                type: ColorPickerRenderMessageType.InitImageData,
                payload: undefined,
            };
            break;
        case ColorPickerRenderMessageType.PutImageData:
            const color = await handlePutImageData(data);
            message = {
                type: ColorPickerRenderMessageType.PutImageData,
                payload: color,
            };
            break;
        case ColorPickerRenderMessageType.GetPreviewImageData:
            const imageData = await handleGetPreviewImageData();
            message = {
                type: ColorPickerRenderMessageType.GetPreviewImageData,
                payload: {
                    imageData,
                },
            };
            break;
        case ColorPickerRenderMessageType.SwitchCaptureHistory:
            await handleSwitchCaptureHistory(data);
            message = {
                type: ColorPickerRenderMessageType.SwitchCaptureHistory,
                payload: undefined,
            };
            break;
        case ColorPickerRenderMessageType.PickColor:
            const pickColorResult = await handlePickColor(data);
            message = {
                type: ColorPickerRenderMessageType.PickColor,
                payload: pickColorResult,
            };
            break;
    }

    self.postMessage(message);
};

self.onabort = () => {
    terminateWorkerAction();
};
