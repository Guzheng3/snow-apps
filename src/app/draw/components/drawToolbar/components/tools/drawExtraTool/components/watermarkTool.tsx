import {
    AppSettingsActionContext,
    AppSettingsGroup,
    AppSettingsPublisher,
} from '@/app/contextWrap';
import { WatermarkProps } from '@/app/draw/components/baseLayer/baseLayerRenderActions';
import {
    CaptureEvent,
    CaptureEventParams,
    CaptureEventPublisher,
    DrawEvent,
    DrawEventParams,
    DrawEventPublisher,
} from '@/app/draw/extra';
import { DrawContext } from '@/app/draw/types';
import {
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventParams,
    ExcalidrawEventPublisher,
} from '@/app/fullScreenDraw/components/drawCore/extra';
import { useCallbackRender } from '@/hooks/useCallbackRender';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { ExcalidrawWatermarkElement } from '@mg-chao/excalidraw/element/types';
import { AppState } from '@mg-chao/excalidraw/types';
import { useCallback, useContext, useRef } from 'react';

const generateWatermarkElement = (text: string, appState: AppState): ExcalidrawWatermarkElement => {
    return {
        id: `snow-shot_watermark_${new Date().valueOf()}`,
        type: 'watermark',
        x: -Number.MAX_SAFE_INTEGER,
        y: -Number.MAX_SAFE_INTEGER,
        width: 0,
        height: 0,
        strokeColor: appState.currentItemStrokeColor,
        backgroundColor: 'transparent',
        fillStyle: 'solid',
        strokeWidth: 2,
        strokeStyle: 'solid',
        roughness: 1,
        opacity: appState.currentItemOpacity,
        watermarkFontSize: appState.currentItemFontSize,
        groupIds: [],
        frameId: null,
        index: null,
        seed: 0,
        version: 1159,
        versionNonce: 2123386168,
        updated: 0,
        roundness: {
            type: 2,
        },
        isDeleted: false,
        boundElements: null,
        link: null,
        locked: false,
        watermarkText: text,
    } as unknown as ExcalidrawWatermarkElement;
};

export const defaultWatermarkProps = {
    fontSize: 0,
    color: '#000000',
    opacity: 0,
    visible: false,
    text: '',
    selectRectParams: {
        rect: {
            min_x: 0,
            min_y: 0,
            max_x: 0,
            max_y: 0,
        },
        radius: 0,
        shadowWidth: 0,
        shadowColor: '#000000',
    },
};

const isEqualWatermarkProps = (a: WatermarkProps, b: WatermarkProps) => {
    return (
        a.fontSize === b.fontSize &&
        a.opacity === b.opacity &&
        a.visible === b.visible &&
        a.color === b.color &&
        a.text === b.text
    );
};

export const WatermarkTool = () => {
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const { drawCacheLayerActionRef, drawLayerActionRef, selectLayerActionRef } =
        useContext(DrawContext);

    // watermark 的绘制所需的属性
    const watermarkPropsRef = useRef<WatermarkProps>(defaultWatermarkProps);

    const createWatermark = useCallback((): boolean => {
        // 获取当前场景里的元素，判断是否存在 watermark 元素
        // 如果存在则忽略

        const excalidrawAPI = drawCacheLayerActionRef.current?.getExcalidrawAPI();
        if (!excalidrawAPI) {
            return false;
        }

        const sceneElements = excalidrawAPI.getSceneElements();
        const appState = excalidrawAPI.getAppState();
        const existWatermarkElement = sceneElements.find((element) => element.type === 'watermark');
        if (existWatermarkElement) {
            return false;
        }

        // 创建 watermark 元素
        const watermarkElement = generateWatermarkElement(
            getAppSettings()[AppSettingsGroup.Cache].lastWatermarkText,
            appState,
        );
        excalidrawAPI.updateScene({
            elements: [...sceneElements, watermarkElement],
            captureUpdate: 'IMMEDIATELY',
        });

        return true;
    }, [drawCacheLayerActionRef, getAppSettings]);

    const updateWatermarkCore = useCallback(
        (appState: AppState) => {
            const selectRectParams = selectLayerActionRef.current?.getSelectRectParams();
            if (!selectRectParams) {
                return;
            }

            const excalidrawAPI = drawCacheLayerActionRef.current?.getExcalidrawAPI();
            if (!excalidrawAPI) {
                return;
            }

            const sceneElements = excalidrawAPI.getSceneElements();

            let targetProps: WatermarkProps;
            const watermarkElement = sceneElements.find((element) => element.type === 'watermark');
            if (watermarkElement) {
                if (appState.activeTool.type === 'watermark') {
                    targetProps = {
                        fontSize: appState.currentItemFontSize,
                        color: appState.currentItemStrokeColor,
                        opacity: appState.currentItemOpacity,
                        visible: true,
                        text: watermarkElement.watermarkText,
                        selectRectParams,
                    };
                } else {
                    targetProps = {
                        fontSize: watermarkElement.watermarkFontSize,
                        color: watermarkElement.strokeColor,
                        opacity: watermarkElement.opacity,
                        visible: true,
                        text: watermarkElement.watermarkText,
                        selectRectParams,
                    };
                }
            } else {
                targetProps = defaultWatermarkProps;
            }

            if (isEqualWatermarkProps(targetProps, watermarkPropsRef.current)) {
                return;
            }

            if (watermarkElement) {
                // 更新 watermark 元素
                excalidrawAPI.updateScene({
                    elements: sceneElements.map((element) => {
                        if (element.type === 'watermark') {
                            return {
                                ...element,
                                watermarkFontSize: targetProps.fontSize,
                                strokeColor: targetProps.color,
                                opacity: targetProps.opacity,
                                watermarkText: targetProps.text,
                            };
                        }

                        return element;
                    }),
                    captureUpdate: 'IMMEDIATELY',
                });
            }

            watermarkPropsRef.current = targetProps;
            drawLayerActionRef.current?.updateWatermarkSprite(watermarkPropsRef.current);
        },
        [drawCacheLayerActionRef, drawLayerActionRef, selectLayerActionRef],
    );
    const updateWatermark = useCallbackRender(updateWatermarkCore);

    // watermark 的样式
    // 样式发生变化时，则可能需要重新创建 watermark element
    const watermarkStyleRef = useRef<{
        text: string;
        opacity: number;
        fontSize: number;
        color: string;
    }>({
        text: defaultWatermarkProps.text,
        opacity: defaultWatermarkProps.opacity,
        fontSize: defaultWatermarkProps.fontSize,
        color: defaultWatermarkProps.color,
    });

    const updateWatermarkText = useCallback(
        (text: string) => {
            if (watermarkStyleRef.current.text !== text) {
                watermarkStyleRef.current.text = text;
                // 如果 watermark 不存在，则创建 watermark
                if (createWatermark()) {
                    return;
                }
            }

            const excalidrawAPI = drawCacheLayerActionRef.current?.getExcalidrawAPI();
            if (!excalidrawAPI) {
                return;
            }

            const sceneElements = excalidrawAPI.getSceneElements();
            if (!sceneElements) {
                return;
            }

            const watermarkElement = sceneElements.find((element) => element.type === 'watermark');

            if (watermarkElement?.watermarkText === text) {
                return;
            }

            excalidrawAPI.updateScene({
                elements: sceneElements.map((element) => {
                    if (element.type === 'watermark') {
                        return { ...element, watermarkText: text };
                    }
                    return element;
                }),
                captureUpdate: 'IMMEDIATELY',
            });

            updateAppSettings(
                AppSettingsGroup.Cache,
                {
                    lastWatermarkText: text,
                },
                true,
                true,
                false,
                true,
                false,
            );
        },
        [createWatermark, drawCacheLayerActionRef, updateAppSettings],
    );

    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState) => {
                if (drawState === DrawState.Watermark) {
                    createWatermark();
                }
            },
            [createWatermark],
        ),
    );
    useStateSubscriber(
        ExcalidrawEventPublisher,
        useCallback(
            (params: ExcalidrawEventParams | undefined) => {
                if (params?.event === 'onChange') {
                    if (
                        watermarkStyleRef.current.opacity !==
                            params.params.appState.currentItemOpacity ||
                        watermarkStyleRef.current.fontSize !==
                            params.params.appState.currentItemFontSize ||
                        watermarkStyleRef.current.color !==
                            params.params.appState.currentItemStrokeColor
                    ) {
                        watermarkStyleRef.current.opacity =
                            params.params.appState.currentItemOpacity;
                        watermarkStyleRef.current.fontSize =
                            params.params.appState.currentItemFontSize;
                        watermarkStyleRef.current.color =
                            params.params.appState.currentItemStrokeColor;

                        if (
                            params.params.appState.activeTool.type === 'watermark' &&
                            createWatermark()
                        ) {
                            return;
                        }
                    }

                    updateWatermark(params.params.appState);
                } else if (params?.event === 'onWatermarkTextChange') {
                    updateWatermarkText(params.params.text);
                }
            },
            [createWatermark, updateWatermark, updateWatermarkText],
        ),
    );
    useStateSubscriber(
        CaptureEventPublisher,
        useCallback(
            (params: CaptureEventParams | undefined) => {
                if (
                    params?.event === CaptureEvent.onExecuteScreenshot ||
                    params?.event === CaptureEvent.onCaptureFinish
                ) {
                    watermarkPropsRef.current = defaultWatermarkProps;
                    drawLayerActionRef.current?.updateWatermarkSprite(watermarkPropsRef.current);
                }
            },
            [drawLayerActionRef],
        ),
    );

    useStateSubscriber(
        DrawEventPublisher,
        useCallback(
            (params: DrawEventParams | undefined) => {
                if (params?.event === DrawEvent.SelectRectParamsAnimationChange) {
                    watermarkPropsRef.current = {
                        ...watermarkPropsRef.current,
                        selectRectParams: params.params.selectRectParams,
                    };
                    drawLayerActionRef.current?.updateWatermarkSprite(watermarkPropsRef.current);
                }
                if (params?.event === DrawEvent.ClearContext) {
                    watermarkPropsRef.current = defaultWatermarkProps;
                }
            },
            [drawLayerActionRef],
        ),
    );

    return <></>;
};
