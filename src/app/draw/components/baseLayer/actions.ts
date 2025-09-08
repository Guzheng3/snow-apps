import { Application, ApplicationOptions, Container, Texture, ICanvas } from 'pixi.js';
import { RefObject } from 'react';
import {
    renderClearCanvasAction,
    renderCreateNewCanvasContainerAction,
    renderDisposeCanvasAction,
    renderGetImageDataAction,
    renderInitCanvasAction,
    renderResizeCanvasAction,
    renderCanvasRenderAction,
    renderAddImageToContainerAction,
    renderClearContainerAction,
    renderCreateBlurSpriteAction,
    BlurSprite,
    BlurSpriteProps,
    renderUpdateBlurSpriteAction,
    renderDeleteBlurSpriteAction,
    renderRenderToCanvasAction,
    WatermarkProps,
    renderUpdateWatermarkSpriteAction,
} from './baseLayerRenderActions';
import {
    BaseLayerRenderDisposeData,
    BaseLayerRenderCreateNewCanvasContainerData,
    BaseLayerRenderInitData,
    BaseLayerRenderMessageType,
    RenderResult,
    BaseLayerRenderResizeCanvasData,
    BaseLayerRenderClearCanvasData,
    BaseLayerRenderGetImageDataData,
    BaseLayerRenderCanvasRenderData,
    BaseLayerRenderAddImageToContainerData,
    BaseLayerRenderClearContainerData,
    BaseLayerRenderCreateBlurSpriteData,
    BaseLayerRenderUpdateBlurSpriteData,
    BaseLayerRenderDeleteBlurSpriteData,
    BaseLayerRenderRenderToCanvasData,
    BaseLayerRenderUpdateWatermarkSpriteData,
} from './workers/renderWorkerTypes';
import { ElementRect } from '@/commands';

export const INIT_CONTAINER_KEY = 'init_container';

export const initCanvasAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    appOptions: Partial<ApplicationOptions>,
    transfer: Transferable[] | undefined,
): Promise<OffscreenCanvas | HTMLCanvasElement | undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.Init) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const InitData: BaseLayerRenderInitData = {
                type: BaseLayerRenderMessageType.Init,
                payload: {
                    appOptions,
                },
            };

            renderWorker.postMessage(InitData, transfer);
        } else {
            renderInitCanvasAction(canvasAppRef, appOptions).then((canvas) => {
                resolve(canvas);
            });
        }
    });
};

export const disposeCanvasAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.Dispose) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const DisposeData: BaseLayerRenderDisposeData = {
                type: BaseLayerRenderMessageType.Dispose,
            };

            renderWorker.postMessage(DisposeData);
        } else {
            renderDisposeCanvasAction(canvasAppRef);
            resolve(undefined);
        }
    });
};

export const createNewCanvasContainerAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    canvasContainerListRef: RefObject<Map<string, Container>>,
    containerKey: string,
): Promise<string | undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.CreateNewCanvasContainer) {
                    resolve(payload.containerKey);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const CreateNewCanvasContainerData: BaseLayerRenderCreateNewCanvasContainerData = {
                type: BaseLayerRenderMessageType.CreateNewCanvasContainer,
                payload: {
                    containerKey: containerKey,
                },
            };

            renderWorker.postMessage(CreateNewCanvasContainerData);
        } else {
            const result = renderCreateNewCanvasContainerAction(
                canvasAppRef,
                canvasContainerListRef,
                containerKey,
            );
            resolve(result);
        }
    });
};

export const resizeCanvasAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    width: number,
    height: number,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.ResizeCanvas) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const ResizeCanvasData: BaseLayerRenderResizeCanvasData = {
                type: BaseLayerRenderMessageType.ResizeCanvas,
                payload: {
                    width: width,
                    height: height,
                },
            };

            renderWorker.postMessage(ResizeCanvasData);
        } else {
            renderResizeCanvasAction(canvasAppRef, width, height);
            resolve(undefined);
        }
    });
};

export const clearCanvasAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    canvasContainerMapRef: RefObject<Map<string, Container>>,
    canvasContainerChildCountRef: RefObject<number>,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.ClearCanvas) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const ClearCanvasData: BaseLayerRenderClearCanvasData = {
                type: BaseLayerRenderMessageType.ClearCanvas,
            };

            renderWorker.postMessage(ClearCanvasData);
        } else {
            renderClearCanvasAction(
                canvasAppRef,
                canvasContainerMapRef,
                canvasContainerChildCountRef,
            );
            resolve(undefined);
        }
    });
};

export const renderToCanvasAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    selectRect: ElementRect,
): Promise<ICanvas | undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.RenderToCanvas) {
                    resolve(payload.canvas);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const RenderToCanvasData: BaseLayerRenderRenderToCanvasData = {
                type: BaseLayerRenderMessageType.RenderToCanvas,
                payload: {
                    selectRect: selectRect,
                },
            };

            renderWorker.postMessage(RenderToCanvasData);
        } else {
            const result = renderRenderToCanvasAction(canvasAppRef, selectRect);
            resolve(result);
        }
    });
};

export const getImageDataAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    selectRect: ElementRect,
): Promise<ImageData | undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.GetImageData) {
                    resolve(payload.imageData);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const GetImageDataData: BaseLayerRenderGetImageDataData = {
                type: BaseLayerRenderMessageType.GetImageData,
                payload: {
                    selectRect: selectRect,
                },
            };

            renderWorker.postMessage(GetImageDataData);
        } else {
            const result = renderGetImageDataAction(canvasAppRef, selectRect);
            resolve(result);
        }
    });
};

export const canvasRenderAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.CanvasRender) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const CanvasRenderData: BaseLayerRenderCanvasRenderData = {
                type: BaseLayerRenderMessageType.CanvasRender,
            };

            renderWorker.postMessage(CanvasRenderData);
        } else {
            renderCanvasRenderAction(canvasAppRef);
            resolve(undefined);
        }
    });
};

export const addImageToContainerAction = async (
    renderWorker: Worker | undefined,
    canvasContainerMapRef: RefObject<Map<string, Container>>,
    currentImageTextureRef: RefObject<Texture | undefined>,
    containerKey: string,
    imageSrc: string,
): Promise<undefined> => {
    return new Promise(async (resolve) => {
        if (renderWorker) {
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.AddImageToContainer) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            const AddImageToContainerData: BaseLayerRenderAddImageToContainerData = {
                type: BaseLayerRenderMessageType.AddImageToContainer,
                payload: {
                    containerKey: containerKey,
                    imageSrc: imageSrc,
                },
            };

            renderWorker.postMessage(AddImageToContainerData);
        } else {
            await renderAddImageToContainerAction(
                canvasContainerMapRef,
                currentImageTextureRef,
                containerKey,
                imageSrc,
            );
            resolve(undefined);
        }
    });
};

export const clearContainerAction = async (
    renderWorker: Worker | undefined,
    canvasContainerMapRef: RefObject<Map<string, Container>>,
    containerKey: string,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const ClearContainerData: BaseLayerRenderClearContainerData = {
                type: BaseLayerRenderMessageType.ClearContainer,
                payload: {
                    containerKey: containerKey,
                },
            };
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.ClearContainer) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            renderWorker.postMessage(ClearContainerData);
        } else {
            renderClearContainerAction(canvasContainerMapRef, containerKey);
            resolve(undefined);
        }
    });
};

export const createBlurSpriteAction = async (
    renderWorker: Worker | undefined,
    canvasContainerMapRef: RefObject<Map<string, Container>>,
    currentImageTextureRef: RefObject<Texture | undefined>,
    blurSpriteMapRef: RefObject<Map<string, BlurSprite>>,
    blurContainerKey: string,
    blurElementId: string,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const CreateBlurSpriteData: BaseLayerRenderCreateBlurSpriteData = {
                type: BaseLayerRenderMessageType.CreateBlurSprite,
                payload: {
                    blurContainerKey: blurContainerKey,
                    blurElementId: blurElementId,
                },
            };
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.CreateBlurSprite) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };
            renderWorker.addEventListener('message', handleMessage);

            renderWorker.postMessage(CreateBlurSpriteData);
        } else {
            renderCreateBlurSpriteAction(
                canvasContainerMapRef,
                currentImageTextureRef,
                blurSpriteMapRef,
                blurContainerKey,
                blurElementId,
            );
            resolve(undefined);
        }
    });
};

export const updateBlurSpriteAction = async (
    renderWorker: Worker | undefined,
    blurSpriteMapRef: RefObject<Map<string, BlurSprite>>,
    blurElementId: string,
    blurProps: BlurSpriteProps,
    updateFilter: boolean,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const UpdateBlurSpriteData: BaseLayerRenderUpdateBlurSpriteData = {
                type: BaseLayerRenderMessageType.UpdateBlurSprite,
                payload: {
                    blurElementId: blurElementId,
                    blurProps: blurProps,
                    updateFilter: updateFilter,
                },
            };
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.UpdateBlurSprite) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };

            renderWorker.addEventListener('message', handleMessage);

            renderWorker.postMessage(UpdateBlurSpriteData);
        } else {
            renderUpdateBlurSpriteAction(blurSpriteMapRef, blurElementId, blurProps, updateFilter);
            resolve(undefined);
        }
    });
};

export const deleteBlurSpriteAction = async (
    renderWorker: Worker | undefined,
    blurSpriteMapRef: RefObject<Map<string, BlurSprite>>,
    blurElementId: string,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const DeleteBlurSpriteData: BaseLayerRenderDeleteBlurSpriteData = {
                type: BaseLayerRenderMessageType.DeleteBlurSprite,
                payload: {
                    blurElementId: blurElementId,
                },
            };
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.DeleteBlurSprite) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };

            renderWorker.addEventListener('message', handleMessage);

            renderWorker.postMessage(DeleteBlurSpriteData);
        } else {
            renderDeleteBlurSpriteAction(blurSpriteMapRef, blurElementId);
            resolve(undefined);
        }
    });
};

export const updateWatermarkSpriteAction = async (
    renderWorker: Worker | undefined,
    canvasAppRef: RefObject<Application | undefined>,
    canvasContainerMapRef: RefObject<Map<string, Container>>,
    lastWatermarkPropsRef: RefObject<WatermarkProps>,
    watermarkContainerKey: string,
    watermarkProps: WatermarkProps,
    textResolution: number,
): Promise<undefined> => {
    return new Promise((resolve) => {
        if (renderWorker) {
            const UpdateWatermarkSpriteData: BaseLayerRenderUpdateWatermarkSpriteData = {
                type: BaseLayerRenderMessageType.UpdateWatermarkSprite,
                payload: {
                    watermarkContainerKey: watermarkContainerKey,
                    watermarkProps: watermarkProps,
                    textResolution: textResolution,
                },
            };
            const handleMessage = (event: MessageEvent<RenderResult>) => {
                const { type, payload } = event.data;
                if (type === BaseLayerRenderMessageType.UpdateWatermarkSprite) {
                    resolve(payload);
                    renderWorker.removeEventListener('message', handleMessage);
                }
            };

            renderWorker.addEventListener('message', handleMessage);

            renderWorker.postMessage(UpdateWatermarkSpriteData);
        } else {
            renderUpdateWatermarkSpriteAction(
                canvasAppRef,
                canvasContainerMapRef,
                watermarkContainerKey,
                lastWatermarkPropsRef,
                watermarkProps,
                textResolution,
            );
            resolve(undefined);
        }
    });
};
