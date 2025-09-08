import { AppSettingsGroup, AppSettingsPublisher } from '@/app/contextWrap';
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

export const WATERMARK_ELEMENT_ID = 'snow-shot_watermark';

const generateWatermarkElement = (text: string, appState: AppState): ExcalidrawWatermarkElement => {
    return {
        id: WATERMARK_ELEMENT_ID,
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
    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const { drawCacheLayerActionRef, drawLayerActionRef, selectLayerActionRef } =
        useContext(DrawContext);

    const watermarkPropsRef = useRef<WatermarkProps>(defaultWatermarkProps);

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

            let targetProps: WatermarkProps | undefined;
            const watermarkElement = sceneElements.find((element) => element.type === 'watermark');
            if (!watermarkElement) {
                targetProps = defaultWatermarkProps;
            } else {
                targetProps = {
                    fontSize: appState.currentItemFontSize,
                    color: appState.currentItemStrokeColor,
                    opacity: appState.currentItemOpacity,
                    visible: true,
                    text: watermarkElement.watermarkText,
                    selectRectParams,
                };
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
                                watermarkFontSize: appState.currentItemFontSize,
                                strokeColor: appState.currentItemStrokeColor,
                                opacity: appState.currentItemOpacity,
                                watermarkText: watermarkElement.watermarkText,
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

    const createWatermark = useCallback(() => {
        // 获取当前场景里的元素，判断是否存在 watermark 元素
        // 如果存在则忽略

        const excalidrawAPI = drawCacheLayerActionRef.current?.getExcalidrawAPI();
        if (!excalidrawAPI) {
            return;
        }

        const sceneElements = excalidrawAPI.getSceneElements();
        const appState = excalidrawAPI.getAppState();
        const existWatermarkElement = sceneElements.find((element) => element.type === 'watermark');
        if (existWatermarkElement) {
            return;
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

        updateWatermark(appState);
    }, [drawCacheLayerActionRef, getAppSettings, updateWatermark]);

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
                if (
                    params?.event === 'onChange' &&
                    params.params.appState.activeTool.type === 'watermark'
                ) {
                    console.log('onChange', params.params.appState);
                    updateWatermark(params.params.appState);
                }
            },
            [updateWatermark],
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
                if (params?.event === DrawEvent.SelectRectParamsChange) {
                    watermarkPropsRef.current = {
                        ...watermarkPropsRef.current,
                        selectRectParams: params.params.selectRectParams,
                    };
                    drawLayerActionRef.current?.updateWatermarkSprite(watermarkPropsRef.current);
                }
            },
            [drawLayerActionRef],
        ),
    );

    return <></>;
};
