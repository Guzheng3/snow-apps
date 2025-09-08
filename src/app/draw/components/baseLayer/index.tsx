'use client';

import React, {
    ComponentType,
    useCallback,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
    useState,
} from 'react';
import * as PIXI from 'pixi.js';
import { ElementRect, ImageBuffer } from '@/commands';
import styles from './index.module.css';
import { releaseDrawPage } from '@/functions/screenshot';
import { CaptureBoundingBoxInfo } from '../../extra';
import {
    clearCanvasAction,
    createNewCanvasContainerAction,
    disposeCanvasAction,
    getImageDataAction,
    initCanvasAction,
    resizeCanvasAction,
    canvasRenderAction,
    addImageToContainerAction,
    INIT_CONTAINER_KEY,
    clearContainerAction,
    createBlurSpriteAction,
    updateBlurSpriteAction,
    deleteBlurSpriteAction,
    renderToCanvasAction,
    updateWatermarkSpriteAction,
} from './actions';
import { supportOffscreenCanvas } from '@/utils';
import { BlurSprite, BlurSpriteProps, WatermarkProps } from './baseLayerRenderActions';
import { appError } from '@/utils/log';
import { DRAW_LAYER_WATERMARK_CONTAINER_KEY } from '../drawLayer';
import { defaultWatermarkProps } from '../drawToolbar/components/tools/drawExtraTool/components/watermarkTool';

export type BaseLayerContextType = {
    /** 调整画布大小 */
    resizeCanvas: (width: number, height: number) => void;
    /** 创建一个新的画布容器 */
    createNewCanvasContainer: (containerKey: string) => Promise<string | undefined>;
    /** 清空画布 */
    clearCanvas: () => void;
    /** 是否启用 */
    isEnable: boolean;
    /**
     * 改变光标样式
     */
    changeCursor: (cursor: Required<React.CSSProperties>['cursor']) => string;
    /** 画布容器元素 */
    layerContainerElementRef: React.RefObject<HTMLDivElement | null>;
    /** 获取图片数据 */
    getImageData: (selectRect: ElementRect) => Promise<ImageData | undefined>;
    /**
     * 渲染画布
     */
    canvasRender: () => Promise<void>;
    /**
     * 添加图片到画布容器
     */
    addImageToContainer: (containerKey: string, imageSrc: string) => Promise<void>;
    /**
     * 清空画布容器
     */
    clearContainer: (containerKey: string) => Promise<void>;
    /**
     * 创建模糊效果
     */
    createBlurSprite: (blurContainerKey: string, blurElementId: string) => Promise<void>;
    /**
     * 更新模糊效果
     */
    updateBlurSprite: (
        blurElementId: string,
        blurProps: BlurSpriteProps,
        updateFilter: boolean,
    ) => Promise<void>;
    /**
     * 删除模糊效果
     */
    deleteBlurSprite: (blurElementId: string) => Promise<void>;
    /**
     * 更新水印效果
     */
    updateWatermarkSprite: (watermarkProps: WatermarkProps) => Promise<void>;
};

export const BaseLayerContext = React.createContext<BaseLayerContextType>({
    resizeCanvas: () => {},
    createNewCanvasContainer: () => Promise.resolve(undefined),
    clearCanvas: () => {},
    isEnable: false,
    /**
     * 改变光标样式
     */
    changeCursor: () => 'auto',
    /** 画布容器元素 */
    layerContainerElementRef: { current: null },
    getImageData: () => Promise.resolve(undefined),
    canvasRender: () => Promise.resolve(),
    addImageToContainer: () => Promise.resolve(),
    clearContainer: () => Promise.resolve(),
    createBlurSprite: () => Promise.resolve(),
    updateBlurSprite: () => Promise.resolve(),
    deleteBlurSprite: () => Promise.resolve(),
    updateWatermarkSprite: () => Promise.resolve(),
});

export type BaseLayerEventActionType = {
    /**
     * 初始化画布
     */
    initCanvas: (antialias: boolean) => Promise<void>;
    /**
     * 执行截图
     */
    onExecuteScreenshot: () => Promise<void>;
    /**
     * 画布准备就绪
     */
    onCanvasReady: (app: PIXI.Application) => void;
    /**
     * 截图准备
     */
    onCaptureReady: (
        imageSrc: string,
        imageBuffer: ImageBuffer,
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
    ) => Promise<void>;
    /**
     * 显示器信息准备
     */
    onCaptureBoundingBoxInfoReady: (
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
    ) => Promise<void>;
    /**
     * 截图加载完成
     */
    onCaptureLoad: (
        imageSrc: string,
        imageBuffer: ImageBuffer,
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
    ) => Promise<void>;
    /**
     * 截图完成
     */
    onCaptureFinish: () => Promise<void>;
    /**
     * 获取画布容器元素
     */
    getLayerContainerElement: () => HTMLDivElement | null;
    /**
     * 创建一个新的画布容器
     */
    createNewCanvasContainer: (containerKey: string) => Promise<string | undefined>;
    /**
     * 获取图片数据
     */
    getImageData: (selectRect: ElementRect) => Promise<ImageData | undefined>;
    /**
     * 渲染到画布
     */
    renderToCanvas: (selectRect: ElementRect) => Promise<PIXI.ICanvas | undefined>;
    /**
     * 渲染画布
     */
    canvasRender: () => Promise<void>;
    /**
     * 添加图片到画布容器
     */
    addImageToContainer: (containerKey: string, imageSrc: string) => Promise<void>;
    /**
     * 清空画布容器
     */
    clearContainer: (containerKey: string) => Promise<void>;
    /**
     * 创建模糊效果
     */
    createBlurSprite: (blurContainerKey: string, blurElementId: string) => Promise<void>;
    /**
     * 更新模糊效果
     */
    updateBlurSprite: (
        blurElementId: string,
        blurProps: BlurSpriteProps,
        updateFilter: boolean,
    ) => Promise<void>;
    /**
     * 更新水印效果
     */
    updateWatermarkSprite: (watermarkProps: WatermarkProps) => Promise<void>;
    /**
     * 删除模糊效果
     */
    deleteBlurSprite: (blurElementId: string) => Promise<void>;
};

export type BaseLayerActionType = {
    /**
     * 设置是否启用
     */
    setEnable: (enable: boolean) => void;
    /**
     * 改变光标样式
     */
    changeCursor: (cursor: Required<React.CSSProperties>['cursor']) => string;
} & BaseLayerEventActionType;

export const defaultBaseLayerActions: BaseLayerActionType = {
    initCanvas: () => Promise.resolve(),
    onCanvasReady: () => {},
    onCaptureReady: () => Promise.resolve(),
    onCaptureLoad: () => Promise.resolve(),
    onCaptureFinish: () => Promise.resolve(),
    onCaptureBoundingBoxInfoReady: () => Promise.resolve(),
    setEnable: () => {},
    onExecuteScreenshot: () => Promise.resolve(),
    getLayerContainerElement: () => null,
    changeCursor: () => 'auto',
    createNewCanvasContainer: () => Promise.resolve(undefined),
    getImageData: () => Promise.resolve(undefined),
    renderToCanvas: () => Promise.resolve(undefined),
    canvasRender: () => Promise.resolve(),
    addImageToContainer: () => Promise.resolve(),
    clearContainer: () => Promise.resolve(),
    createBlurSprite: () => Promise.resolve(),
    updateBlurSprite: () => Promise.resolve(),
    deleteBlurSprite: () => Promise.resolve(),
    updateWatermarkSprite: () => Promise.resolve(),
};

type BaseLayerCoreActionType = {
    /**
     * 初始化画布
     */
    initCanvas: (antialias: boolean) => Promise<void>;
    resizeCanvas: (width: number, height: number) => void;
    clearCanvas: () => Promise<void>;
    getLayerContainerElement: () => HTMLDivElement | null;
    changeCursor: (cursor: Required<React.CSSProperties>['cursor']) => string;
    getImageData: (selectRect: ElementRect) => Promise<ImageData | undefined>;
    /**
     * 渲染画布
     */
    renderToCanvas: (selectRect: ElementRect) => Promise<PIXI.ICanvas | undefined>;
    /**
     * 渲染画布
     */
    canvasRender: () => Promise<void>;
    /**
     * 添加图片到画布容器
     */
    addImageToContainer: (containerKey: string, imageSrc: string) => Promise<void>;
    /**
     * 清空画布容器
     */
    clearContainer: (containerKey: string) => Promise<void>;
    /**
     * 创建模糊效果
     */
    createBlurSprite: (blurContainerKey: string, blurElementId: string) => Promise<void>;
    /**
     * 更新模糊效果
     */
    updateBlurSprite: (
        blurElementId: string,
        blurProps: BlurSpriteProps,
        updateFilter: boolean,
    ) => Promise<void>;
    /**
     * 更新水印效果
     */
    updateWatermarkSprite: (watermarkProps: WatermarkProps) => Promise<void>;
    /**
     * 删除模糊效果
     */
    deleteBlurSprite: (blurElementId: string) => Promise<void>;
};

export type BaseLayerProps = {
    children: React.ReactNode;
    zIndex: number;
    enable: boolean;
};

export const BaseLayerCore: React.FC<
    BaseLayerProps & { actionRef?: React.RefObject<BaseLayerCoreActionType | undefined> }
> = ({ zIndex, actionRef, enable, children }) => {
    const layerContainerElementRef = useRef<HTMLDivElement>(null);
    const canvasElementRef = useRef<HTMLCanvasElement>(null);
    /** 可能的 OffscreenCanvas，用于在 Web Worker 中渲染 */
    const offscreenCanvasRef = useRef<OffscreenCanvas | undefined>(undefined);
    const canvasAppRef = useRef<PIXI.Application | undefined>(undefined);
    const canvasContainerMapRef = useRef<Map<string, PIXI.Container>>(new Map());
    const canvasContainerChildCountRef = useRef<number>(0);
    const currentImageTextureRef = useRef<PIXI.Texture | undefined>(undefined);
    const blurSpriteMapRef = useRef<Map<string, BlurSprite>>(new Map());
    const lastWatermarkPropsRef = useRef<WatermarkProps>(defaultWatermarkProps);
    const rendererWorker = useMemo(() => {
        if (supportOffscreenCanvas()) {
            return new Worker(new URL('./workers/renderWorker.ts', import.meta.url));
        }
        return undefined;
    }, []);
    useEffect(() => {
        return () => {
            rendererWorker?.terminate();
        };
    }, [rendererWorker]);

    /** 创建一个新的画布容器 */
    const createNewCanvasContainer = useCallback(
        async (containerKey: string): Promise<string | undefined> => {
            return await createNewCanvasContainerAction(
                rendererWorker,
                canvasAppRef,
                canvasContainerMapRef,
                containerKey,
            );
        },
        [rendererWorker],
    );

    const disposeCanvas = useCallback(async () => {
        await disposeCanvasAction(rendererWorker, canvasAppRef);
    }, [rendererWorker]);

    /** 初始化画布 */
    const initedRef = useRef(false);
    const initCanvas = useCallback<BaseLayerCoreActionType['initCanvas']>(
        async (antialias: boolean) => {
            if (initedRef.current) {
                return;
            }

            if (!canvasElementRef.current) {
                appError('[BaseLayerCore] canvasRef.current is null');
                return;
            }

            offscreenCanvasRef.current = supportOffscreenCanvas()
                ? canvasElementRef.current.transferControlToOffscreen()
                : undefined;

            initedRef.current = true;

            await disposeCanvas();

            const initOptions: Partial<PIXI.ApplicationOptions> = {
                backgroundAlpha: 0,
                eventFeatures: {
                    move: false,
                    globalMove: false,
                    click: false,
                    wheel: false,
                },
                autoStart: false,
                antialias,
                canvas: offscreenCanvasRef.current ?? canvasElementRef.current,
                preference: 'webgl',
            };

            await initCanvasAction(
                rendererWorker,
                canvasAppRef,
                initOptions,
                offscreenCanvasRef.current ? [offscreenCanvasRef.current] : undefined,
            );

            releaseDrawPage();
        },
        [disposeCanvas, rendererWorker],
    );

    /** 调整画布大小 */
    const resizeCanvas = useCallback(
        async (width: number, height: number) => {
            await createNewCanvasContainer(INIT_CONTAINER_KEY);
            await resizeCanvasAction(rendererWorker, canvasAppRef, width, height);
        },
        [createNewCanvasContainer, rendererWorker],
    );

    const clearCanvas = useCallback<BaseLayerCoreActionType['clearCanvas']>(async () => {
        await clearCanvasAction(
            rendererWorker,
            canvasAppRef,
            canvasContainerMapRef,
            canvasContainerChildCountRef,
        );
    }, [rendererWorker]);

    const changeCursor = useCallback<BaseLayerContextType['changeCursor']>((cursor) => {
        const previousCursor = layerContainerElementRef.current!.style.cursor;
        layerContainerElementRef.current!.style.cursor = cursor;
        return previousCursor;
    }, []);

    const getLayerContainerElement = useCallback<
        BaseLayerCoreActionType['getLayerContainerElement']
    >(() => layerContainerElementRef.current, []);

    const getImageData = useCallback<BaseLayerActionType['getImageData']>(
        async (selectRect: ElementRect) => {
            return getImageDataAction(rendererWorker, canvasAppRef, selectRect);
        },
        [rendererWorker],
    );

    const renderToCanvas = useCallback<BaseLayerActionType['renderToCanvas']>(
        async (selectRect: ElementRect) => {
            return renderToCanvasAction(rendererWorker, canvasAppRef, selectRect);
        },
        [rendererWorker],
    );

    const canvasRender = useCallback<BaseLayerActionType['canvasRender']>(async () => {
        await canvasRenderAction(rendererWorker, canvasAppRef);
    }, [rendererWorker]);

    const addImageToContainer = useCallback<BaseLayerActionType['addImageToContainer']>(
        async (containerKey: string, imageSrc: string) => {
            await addImageToContainerAction(
                rendererWorker,
                canvasContainerMapRef,
                currentImageTextureRef,
                containerKey,
                imageSrc,
            );
        },
        [rendererWorker],
    );

    const clearContainer = useCallback<BaseLayerActionType['clearContainer']>(
        async (containerKey: string) => {
            await clearContainerAction(rendererWorker, canvasContainerMapRef, containerKey);
        },
        [rendererWorker],
    );

    const createBlurSprite = useCallback<BaseLayerActionType['createBlurSprite']>(
        async (blurContainerKey: string, blurElementId: string) => {
            await createBlurSpriteAction(
                rendererWorker,
                canvasContainerMapRef,
                currentImageTextureRef,
                blurSpriteMapRef,
                blurContainerKey,
                blurElementId,
            );
        },
        [rendererWorker],
    );

    const updateBlurSprite = useCallback<BaseLayerActionType['updateBlurSprite']>(
        async (blurElementId: string, blurProps: BlurSpriteProps, updateFilter: boolean) => {
            await updateBlurSpriteAction(
                rendererWorker,
                blurSpriteMapRef,
                blurElementId,
                blurProps,
                updateFilter,
            );
        },
        [rendererWorker],
    );

    const deleteBlurSprite = useCallback<BaseLayerActionType['deleteBlurSprite']>(
        async (blurElementId: string) => {
            await deleteBlurSpriteAction(rendererWorker, blurSpriteMapRef, blurElementId);
        },
        [rendererWorker],
    );

    const updateWatermarkSprite = useCallback<BaseLayerActionType['updateWatermarkSprite']>(
        async (watermarkProps: WatermarkProps) => {
            await updateWatermarkSpriteAction(
                rendererWorker,
                canvasAppRef,
                canvasContainerMapRef,
                lastWatermarkPropsRef,
                DRAW_LAYER_WATERMARK_CONTAINER_KEY,
                watermarkProps,
                window.devicePixelRatio,
            );
        },
        [rendererWorker],
    );

    useEffect(() => {
        return () => {
            disposeCanvas();
        };
    }, [initCanvas, disposeCanvas]);

    useImperativeHandle(
        actionRef,
        () => ({
            resizeCanvas,
            clearCanvas,
            getLayerContainerElement,
            changeCursor,
            createNewCanvasContainer,
            getImageData,
            renderToCanvas,
            initCanvas,
            canvasRender,
            addImageToContainer,
            clearContainer,
            createBlurSprite,
            updateBlurSprite,
            deleteBlurSprite,
            updateWatermarkSprite,
        }),
        [
            resizeCanvas,
            clearCanvas,
            getLayerContainerElement,
            changeCursor,
            createNewCanvasContainer,
            getImageData,
            renderToCanvas,
            initCanvas,
            canvasRender,
            addImageToContainer,
            clearContainer,
            createBlurSprite,
            updateBlurSprite,
            deleteBlurSprite,
            updateWatermarkSprite,
        ],
    );

    useEffect(() => {
        if (enable) {
            layerContainerElementRef.current!.style.pointerEvents = 'auto';
        } else {
            layerContainerElementRef.current!.style.pointerEvents = 'none';
        }
    }, [enable]);

    const baseLayerContextValue = useMemo(() => {
        return {
            resizeCanvas,
            clearCanvas,
            createNewCanvasContainer,
            isEnable: enable,
            changeCursor,
            layerContainerElementRef,
            getImageData,
            canvasRender,
            addImageToContainer,
            clearContainer,
            createBlurSprite,
            updateBlurSprite,
            deleteBlurSprite,
            updateWatermarkSprite,
        };
    }, [
        resizeCanvas,
        clearCanvas,
        createNewCanvasContainer,
        enable,
        changeCursor,
        getImageData,
        canvasRender,
        addImageToContainer,
        clearContainer,
        createBlurSprite,
        updateBlurSprite,
        deleteBlurSprite,
        updateWatermarkSprite,
    ]);

    return (
        <BaseLayerContext.Provider value={baseLayerContextValue}>
            <div className={styles.baseLayer} ref={layerContainerElementRef} style={{ zIndex }}>
                <canvas ref={canvasElementRef} />
            </div>
            {children}
        </BaseLayerContext.Provider>
    );
};

/**
 * BaseLayer 高阶组件
 * @param WrappedComponent 需要包装的基础组件
 */
export function withBaseLayer<
    ActionType extends BaseLayerActionType,
    Props extends { actionRef: React.RefObject<ActionType | undefined> },
>(WrappedComponent: ComponentType<Props>, zIndex: number) {
    return React.memo(function BaseLayer(props: Props) {
        const { actionRef } = props;
        const layerActionRef = useRef<ActionType | undefined>(undefined);
        const baseLayerCoreActionRef = useRef<BaseLayerCoreActionType | undefined>(undefined);

        const [layerEnable, setLayerEnable] = useState(false);

        const initCanvas = useCallback<BaseLayerActionType['initCanvas']>(
            async (antialias: boolean) => {
                await baseLayerCoreActionRef.current?.initCanvas(antialias);
            },
            [],
        );
        const onCaptureReady = useCallback(
            async (...args: Parameters<BaseLayerActionType['onCaptureReady']>) => {
                await layerActionRef.current?.onCaptureReady(...args);
            },
            [],
        );
        const onCaptureBoundingBoxInfoReady = useCallback(
            async (...args: Parameters<BaseLayerActionType['onCaptureBoundingBoxInfoReady']>) => {
                const [captureBoundingBoxInfo] = args;

                // 将画布调整为截图大小
                const { width, height } = captureBoundingBoxInfo;

                baseLayerCoreActionRef.current?.resizeCanvas(width, height);
                await layerActionRef.current?.onCaptureBoundingBoxInfoReady(...args);
            },
            [],
        );
        const onCaptureFinish = useCallback(
            async (...args: Parameters<BaseLayerActionType['onCaptureFinish']>) => {
                await Promise.all([
                    baseLayerCoreActionRef.current?.clearCanvas(),
                    layerActionRef.current?.onCaptureFinish(...args),
                ]);
            },
            [],
        );

        const setEnable = useCallback((...args: Parameters<BaseLayerActionType['setEnable']>) => {
            setLayerEnable(...args);
            layerActionRef.current?.setEnable(...args);
        }, []);

        const getLayerContainerElement = useCallback(() => {
            return baseLayerCoreActionRef.current?.getLayerContainerElement();
        }, []);

        const changeCursor = useCallback<BaseLayerActionType['changeCursor']>((cursor) => {
            return baseLayerCoreActionRef.current?.changeCursor(cursor) ?? 'auto';
        }, []);

        const getImageData = useCallback<BaseLayerActionType['getImageData']>(
            async (selectRect: ElementRect) => {
                return baseLayerCoreActionRef.current?.getImageData(selectRect);
            },
            [],
        );

        useImperativeHandle(
            actionRef,
            () => ({
                ...(layerActionRef.current as ActionType),
                onCaptureReady,
                onCaptureFinish,
                onCaptureBoundingBoxInfoReady,
                setEnable,
                getLayerContainerElement,
                changeCursor,
                getImageData,
                initCanvas,
            }),
            [
                onCaptureReady,
                onCaptureFinish,
                onCaptureBoundingBoxInfoReady,
                setEnable,
                getLayerContainerElement,
                changeCursor,
                getImageData,
                initCanvas,
            ],
        );

        return (
            <BaseLayerCore zIndex={zIndex} actionRef={baseLayerCoreActionRef} enable={layerEnable}>
                <WrappedComponent {...props} actionRef={layerActionRef} />
            </BaseLayerCore>
        );
    });
}
