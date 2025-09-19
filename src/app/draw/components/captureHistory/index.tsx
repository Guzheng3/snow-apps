import { useCallback, useContext, useImperativeHandle, useRef } from 'react';
import { DrawContext } from '../../types';
import { CaptureHistory } from '@/utils/captureHistory';
import { CaptureHistoryItem } from '@/utils/appStore';
import { AppSettingsData, AppSettingsPublisher } from '@/app/contextWrap';
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
    ScreenshotTypePublisher,
} from '../../extra';
import { Ordered } from '@mg-chao/excalidraw/element/types';
import { NonDeletedExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { AntdContext } from '@/components/globalLayoutExtra';
import { FormattedMessage } from 'react-intl';
import { appError } from '@/utils/log';
import { ImageBuffer } from '@/commands';
import { ScreenshotType } from '@/functions/screenshot';

export type CaptureHistoryActionType = {
    saveCurrentCapture: (imageBuffer: ImageBuffer | undefined) => Promise<void>;
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
                if (captureEvent?.event === CaptureEvent.onExecuteScreenshot) {
                    isImageLoadingRef.current = true;
                } else if (captureEvent?.event === CaptureEvent.onCaptureReady) {
                    reloadCaptureHistoryList();
                } else if (captureEvent?.event === CaptureEvent.onCaptureLoad) {
                    isImageLoadingRef.current = false;
                }
            },
            [reloadCaptureHistoryList],
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

    const currentCaptureExcalidrawElementsRef =
        useRef<readonly Ordered<NonDeletedExcalidrawElement>[]>(undefined);
    const changeCurrentIndex = useCallback(
        async (delta: number) => {
            if (getScreenshotType() === ScreenshotType.TopWindow) {
                return;
            }

            if (captureHistoryListRef.current.length === 0) {
                return;
            }

            if (isImageLoadingRef.current) {
                return;
            }

            const newIndex = Math.max(
                0,
                Math.min(currentIndexRef.current + delta, captureHistoryListRef.current.length),
            );

            if (newIndex === currentIndexRef.current) {
                return;
            }

            currentIndexRef.current = newIndex;

            isImageLoadingRef.current = true;

            const hideLoading = message.loading({
                content: <FormattedMessage id="draw.loadingCaptureHistory" />,
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
        ],
    );

    const saveCurrentCapture = useCallback(
        async (imageBuffer: ImageBuffer | undefined) => {
            if (!captureHistoryRef.current || !imageBuffer) {
                appError('[CaptureHistoryController] saveCurrentCapture error, invalid state', {
                    captureHistoryRef: captureHistoryRef.current,
                    imageBuffer: imageBuffer,
                });
                return;
            }

            const selectRect = selectLayerActionRef.current?.getSelectRect();
            if (!selectRect) {
                appError(
                    '[CaptureHistoryController] saveCurrentCapture error, invalid selectRect',
                    {
                        selectRect: selectRect,
                    },
                );
                return;
            }

            const excalidrawApi = drawCacheLayerActionRef.current?.getExcalidrawAPI();

            await captureHistoryRef.current.save(
                captureHistoryListRef.current[currentIndexRef.current] ?? imageBuffer,
                excalidrawApi?.getSceneElements(),
                excalidrawApi?.getAppState(),
                selectRect,
            );
        },
        [drawCacheLayerActionRef, selectLayerActionRef],
    );

    useImperativeHandle(actionRef, () => {
        return {
            saveCurrentCapture,
        };
    }, [saveCurrentCapture]);

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
