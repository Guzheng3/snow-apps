import { useCallback, useContext, useImperativeHandle, useRef } from 'react';
import { DrawContext } from '../../types';
import { CaptureHistory, getCaptureHistoryImageAbsPath } from '@/utils/captureHistory';
import { CaptureHistoryItem, CaptureHistorySource } from '@/utils/appStore';
import { AppSettingsData, AppSettingsGroup, AppSettingsPublisher } from '@/app/contextWrap';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { KeyEventWrap } from '../drawToolbar/components/keyEventWrap';
import { KeyEventKey } from '../drawToolbar/components/keyEventWrap/extra';
import React from 'react';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { EnableKeyEventPublisher } from '../drawToolbar/components/keyEventWrap/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import {
    CaptureEvent,
    CaptureEventParams,
    CaptureEventPublisher,
    DrawEvent,
    DrawEventPublisher,
    ScreenshotTypePublisher,
} from '../../extra';
import { Ordered } from '@mg-chao/excalidraw/element/types';
import { NonDeletedExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { AntdContext } from '@/components/globalLayoutExtra';
import { FormattedMessage } from 'react-intl';
import { appError } from '@/utils/log';
import { ImageBuffer } from '@/commands';
import { onCaptureHistoryChange, ScreenshotType } from '@/functions/screenshot';
import { captureFullScreen, CaptureFullScreenResult } from '@/commands/screenshot';
import { getImagePathFromSettings } from '@/utils/file';
import { playCameraShutterSound } from '@/utils/audio';
import { ImageSharedBufferData } from '../../tools';
import { encodeImage } from './workers/encodeImage';
import { AppState } from '@mg-chao/excalidraw/types';
import { ElementRect } from '@/commands';
import { getCorrectHdrColorAlgorithm } from '@/utils/appSettings';

export type CaptureHistoryActionType = {
    saveCurrentCapture: (
        imageBuffer: ImageBuffer | ImageSharedBufferData | undefined,
        selectRect: ElementRect | undefined,
        excalidrawElements: readonly Ordered<NonDeletedExcalidrawElement>[] | undefined,
        appState: Readonly<AppState> | undefined,
        captureResult?: ArrayBuffer,
        source?: CaptureHistorySource,
    ) => Promise<void>;
    switch: (captureHistoryId: string) => Promise<void>;
    captureFullScreen: () => Promise<void>;
};

const CaptureHistoryControllerCore: React.FC<{
    actionRef: React.RefObject<CaptureHistoryActionType | undefined>;
}> = ({ actionRef }) => {
    const captureHistoryListRef = useRef<CaptureHistoryItem[]>([]);
    const currentIndexRef = useRef<number>(0);
    const captureHistoryRef = useRef<CaptureHistory | undefined>(undefined);
    const isImageLoadingRef = useRef<boolean>(false);
    const [getScreenshotType] = useStateSubscriber(ScreenshotTypePublisher, undefined);
    const [, setEnableKeyEvent] = useStateSubscriber(EnableKeyEventPublisher, undefined);
    const {
        selectLayerActionRef,
        drawLayerActionRef,
        drawCacheLayerActionRef,
        colorPickerActionRef,
    } = useContext(DrawContext);

    const resetCurrentIndex = useCallback(() => {
        currentIndexRef.current = captureHistoryListRef.current.length;
    }, [captureHistoryListRef]);

    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const reloadCaptureHistoryList = useCallback(
        async (appSettings?: AppSettingsData) => {
            if (!captureHistoryRef.current) {
                return;
            }

            captureHistoryListRef.current = await captureHistoryRef.current.getList(
                appSettings ?? getAppSettings(),
            );
            resetCurrentIndex();
        },
        [resetCurrentIndex, getAppSettings],
    );
    const init = useCallback(
        async (appSettings: AppSettingsData) => {
            if (captureHistoryRef.current?.inited()) {
                return;
            }

            captureHistoryRef.current = new CaptureHistory();
            await captureHistoryRef.current.init();

            reloadCaptureHistoryList(appSettings);
        },
        [reloadCaptureHistoryList],
    );

    useStateSubscriber(
        CaptureEventPublisher,
        useCallback(
            (captureEvent: CaptureEventParams | undefined) => {
                if (captureEvent?.event === CaptureEvent.onCaptureFinish) {
                    resetCurrentIndex();
                }
            },
            [resetCurrentIndex],
        ),
    );

    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState) => {
                setEnableKeyEvent(drawState === DrawState.Idle);
            },
            [setEnableKeyEvent],
        ),
    );

    useAppSettingsLoad(
        useCallback(
            (appSettings) => {
                init(appSettings);
            },
            [init],
        ),
        true,
    );

    const { message } = useContext(AntdContext);

    const [, setDrawEvent] = useStateSubscriber(DrawEventPublisher, undefined);

    const currentCaptureExcalidrawElementsRef =
        useRef<readonly Ordered<NonDeletedExcalidrawElement>[]>(undefined);
    const changeCurrentIndex = useCallback(
        async (delta: number | string) => {
            if (getScreenshotType()?.type === ScreenshotType.TopWindow) {
                return;
            }

            if (captureHistoryListRef.current.length === 0) {
                return;
            }

            if (isImageLoadingRef.current) {
                return;
            }

            let newIndex = currentIndexRef.current;
            if (typeof delta === 'number') {
                newIndex = Math.max(
                    0,
                    Math.min(
                        currentIndexRef.current + delta,
                        getScreenshotType()?.type === ScreenshotType.SwitchCaptureHistory
                            ? Math.max(0, captureHistoryListRef.current.length - 1) // 切换截图历史时，不允许切换回截图
                            : captureHistoryListRef.current.length,
                    ),
                );
            } else {
                newIndex = captureHistoryListRef.current.findIndex((item) => item.id === delta);
            }

            if (newIndex === currentIndexRef.current) {
                return;
            }

            currentIndexRef.current = newIndex;

            isImageLoadingRef.current = true;

            const hideLoading = message.loading({
                content: <FormattedMessage id="draw.loadingCaptureHistory" />,
            });

            setDrawEvent({
                event: DrawEvent.ClearContext,
                params: undefined,
            });
            if (currentIndexRef.current === captureHistoryListRef.current.length) {
                const switchCaptureHistoryPromise = Promise.all([
                    drawLayerActionRef.current?.switchCaptureHistory(undefined).then(() => {
                        // 恢复绘制的内容
                        if (currentCaptureExcalidrawElementsRef.current) {
                            drawCacheLayerActionRef.current?.updateScene({
                                elements: currentCaptureExcalidrawElementsRef.current ?? [],
                                captureUpdate: 'NEVER',
                            });
                            drawCacheLayerActionRef.current?.clearHistory();
                            currentCaptureExcalidrawElementsRef.current = undefined;
                        }
                    }),
                    colorPickerActionRef.current?.switchCaptureHistory(undefined),
                ]);

                selectLayerActionRef.current?.switchCaptureHistory(undefined);

                await switchCaptureHistoryPromise;
            } else {
                const switchCaptureHistoryPromise = Promise.all([
                    drawLayerActionRef.current
                        ?.switchCaptureHistory(
                            captureHistoryListRef.current[currentIndexRef.current],
                        )
                        .then(() => {
                            // 等待切换完成后，再更新绘制内容
                            // 避免模糊工具更新时取得错误数据

                            // 保存当前绘制的内容
                            if (currentCaptureExcalidrawElementsRef.current === undefined) {
                                currentCaptureExcalidrawElementsRef.current =
                                    drawCacheLayerActionRef.current
                                        ?.getExcalidrawAPI()
                                        ?.getSceneElements();
                            }

                            drawCacheLayerActionRef.current?.updateScene({
                                elements:
                                    captureHistoryListRef.current[currentIndexRef.current]
                                        .excalidraw_elements ?? [],
                                appState:
                                    captureHistoryListRef.current[currentIndexRef.current]
                                        .excalidraw_app_state,
                                captureUpdate: 'NEVER',
                            });
                        }),
                    colorPickerActionRef.current?.switchCaptureHistory(
                        captureHistoryListRef.current[currentIndexRef.current],
                    ),
                ]);

                selectLayerActionRef.current?.switchCaptureHistory(
                    captureHistoryListRef.current[currentIndexRef.current],
                );

                drawCacheLayerActionRef.current?.clearHistory();

                await switchCaptureHistoryPromise;
            }

            isImageLoadingRef.current = false;

            hideLoading();
        },
        [
            colorPickerActionRef,
            drawCacheLayerActionRef,
            drawLayerActionRef,
            getScreenshotType,
            message,
            selectLayerActionRef,
            setDrawEvent,
        ],
    );

    const saveCurrentCapture = useCallback(
        async (
            imageBuffer: ImageBuffer | ImageSharedBufferData | CaptureFullScreenResult | undefined,
            selectRect: ElementRect | undefined,
            excalidrawElements: readonly Ordered<NonDeletedExcalidrawElement>[] | undefined,
            appState: Readonly<AppState> | undefined,
            captureResult?: ArrayBuffer,
            source?: CaptureHistorySource,
        ) => {
            let sharedBufferEncodeImagePromise: Promise<ArrayBuffer | undefined> =
                Promise.resolve(undefined);
            if (
                imageBuffer &&
                'sharedBuffer' in imageBuffer &&
                !captureHistoryListRef.current[currentIndexRef.current]
            ) {
                sharedBufferEncodeImagePromise = encodeImage(
                    imageBuffer.width,
                    imageBuffer.height,
                    imageBuffer.sharedBuffer,
                );
            }

            if (!captureHistoryRef.current) {
                appError('[CaptureHistoryController] saveCurrentCapture error, invalid state', {
                    captureHistoryRef: captureHistoryRef.current,
                });
                return;
            }

            if (!imageBuffer && getScreenshotType()?.type !== ScreenshotType.SwitchCaptureHistory) {
                appError(
                    '[CaptureHistoryController] saveCurrentCapture error, invalid imageBuffer',
                    {
                        imageBuffer: imageBuffer,
                    },
                );
                return;
            }

            if (!selectRect) {
                appError(
                    '[CaptureHistoryController] saveCurrentCapture error, invalid selectRect',
                    {
                        selectRect: selectRect,
                    },
                );
                return;
            }

            const sharedBufferEncodeImage = await sharedBufferEncodeImagePromise;
            const captureHistoryItem = await captureHistoryRef.current.save(
                captureHistoryListRef.current[currentIndexRef.current] ??
                    (sharedBufferEncodeImage
                        ? {
                              encodeData: sharedBufferEncodeImage,
                          }
                        : imageBuffer),
                excalidrawElements,
                appState,
                selectRect,
                captureResult,
                source,
            );
            captureHistoryListRef.current.push(captureHistoryItem);
            resetCurrentIndex();
            onCaptureHistoryChange();
        },
        [getScreenshotType, resetCurrentIndex],
    );

    const captureFullScreenAction = useCallback(async () => {
        if (!captureHistoryRef.current) {
            appError('[CaptureHistoryController] captureFullScreenAction error, invalid state', {
                captureHistoryRef: captureHistoryRef.current,
            });
            return;
        }

        const appSettings = getAppSettings();
        const captureHistoryParams = CaptureHistory.generateCaptureHistoryItem(
            'full-screen',
            undefined,
            undefined,
            undefined,
            undefined,
            CaptureHistorySource.FullScreen,
        );

        const imagePath = await getImagePathFromSettings(appSettings, 'full-screen');
        if (!imagePath) {
            return;
        }

        let captureFullScreenResult: CaptureFullScreenResult;
        try {
            const captureFullScreenResultPromise = captureFullScreen(
                appSettings[AppSettingsGroup.SystemScreenshot].enableMultipleMonitor,
                imagePath.filePath,
                appSettings[AppSettingsGroup.FunctionScreenshot].fullScreenCopyToClipboard,
                await getCaptureHistoryImageAbsPath(captureHistoryParams.file_name),
                getCorrectHdrColorAlgorithm(appSettings),
                appSettings[AppSettingsGroup.SystemScreenshot].correctColorFilter,
            );
            playCameraShutterSound();
            captureFullScreenResult = await captureFullScreenResultPromise;
        } catch (error) {
            appError('[CaptureHistoryController] captureFullScreenAction error', error);
            return;
        }

        captureHistoryParams.selected_rect = captureFullScreenResult.monitor_rect;
        const captureHistoryItemPromise = captureHistoryRef.current.save(
            {
                type: 'full-screen',
                captureHistoryItem: captureHistoryParams,
            },
            undefined,
            undefined,
            captureFullScreenResult.monitor_rect,
            undefined,
            CaptureHistorySource.FullScreen,
        );
        const captureHistoryItem = await captureHistoryItemPromise;
        captureHistoryListRef.current.push(captureHistoryItem);
        resetCurrentIndex();
        onCaptureHistoryChange();
    }, [getAppSettings, resetCurrentIndex]);

    useImperativeHandle(actionRef, () => {
        return {
            saveCurrentCapture,
            switch: changeCurrentIndex,
            captureFullScreen: captureFullScreenAction,
        };
    }, [saveCurrentCapture, changeCurrentIndex, captureFullScreenAction]);

    return (
        <>
            <KeyEventWrap
                componentKey={KeyEventKey.PreviousCapture}
                onKeyDown={() => {
                    changeCurrentIndex(-1);
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.NextCapture}
                onKeyDown={() => {
                    changeCurrentIndex(1);
                }}
            >
                <div />
            </KeyEventWrap>
        </>
    );
};

export const CaptureHistoryController = React.memo(
    withStatePublisher(CaptureHistoryControllerCore, EnableKeyEventPublisher),
);
