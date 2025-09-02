'use client';

import { createDrawWindow, ElementRect, getMousePosition, ImageBuffer } from '@/commands';
import { EventListenerContext } from '@/components/eventListener';
import React, { useMemo, useState } from 'react';
import { useCallback, useContext, useEffect, useRef } from 'react';
import { CanvasLayer, CaptureStep, DrawContext, DrawContextType } from './types';
import SelectLayer, { SelectLayerActionType } from './components/selectLayer';
import { DrawLayer, DrawLayerActionType } from './components/drawLayer';
import { Window as AppWindow, getCurrentWindow } from '@tauri-apps/api/window';
import {
    CaptureLoadingPublisher,
    CaptureStepPublisher,
    switchLayer,
    CaptureEventPublisher,
    CaptureEvent,
    ScreenshotTypePublisher,
    DrawEventPublisher,
    CaptureBoundingBoxInfo,
} from './extra';
import {
    DrawToolbar,
    DrawToolbarActionType,
    DrawToolbarStatePublisher,
} from './components/drawToolbar';
import { BaseLayerEventActionType } from './components/baseLayer';
import { ColorPicker, ColorPickerActionType } from './components/colorPicker';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import StatusBar from './components/statusBar';
import { MousePosition } from '@/utils/mousePosition';
import { EnableKeyEventPublisher } from './components/drawToolbar/components/keyEventWrap/extra';
import { zIndexs } from '@/utils/zIndex';
import styles from './page.module.css';
import dynamic from 'next/dynamic';
import { DrawCacheLayerActionType } from './components/drawCacheLayer/extra';
import { copyToClipboard, fixedToScreen, getCanvas, handleOcrDetect, saveToFile } from './actions';
import {
    FixedContentActionType,
    FixedContentCore,
} from '../fixedContent/components/fixedContentCore';
import { OcrBlocks, OcrBlocksActionType } from './components/ocrBlocks';
import {
    executeScreenshot as executeScreenshotFunc,
    releaseDrawPage,
    ScreenshotType,
} from '@/functions/screenshot';
import { setWindowRect, showWindow as showCurrentWindow } from '@/utils/window';
import { captureAllMonitors, setDrawWindowStyle, switchAlwaysOnTop } from '@/commands/screenshot';
import { debounce } from 'es-toolkit';
import {
    scrollScreenshotClear,
    scrollScreenshotSaveToClipboard,
} from '@/commands/scrollScreenshot';
import { getImageFormat, getImagePathFromSettings, showImageDialog } from '@/utils/file';
import { scrollScreenshotSaveToFile } from '@/commands/scrollScreenshot';
import { AppSettingsActionContext, AppSettingsGroup } from '../contextWrap';
import { AppSettingsPublisher } from '../contextWrap';
import { ExtraTool } from './components/drawToolbar/components/tools/extraTool';
import {
    createFixedContentWindow,
    getMonitorsBoundingBox,
    setCurrentWindowAlwaysOnTop,
} from '@/commands/core';
import {
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventPublisher,
    ExcalidrawOnHandleEraserPublisher,
} from '../fullScreenDraw/components/drawCore/extra';
import {
    HistoryContext,
    withCanvasHistory,
} from '../fullScreenDraw/components/drawCore/components/historyContext';
import { covertOcrResultToText } from '../fixedContent/components/ocrResult';
import { writeTextToClipboard } from '@/utils/clipboard';
import { listenKeyStart, listenKeyStop } from '@/commands/listenKey';
import { sendErrorMessage } from '@/functions/sendMessage';
import { FormattedMessage, useIntl } from 'react-intl';
import Flatbush from 'flatbush';
import { isOcrTool } from './components/drawToolbar/components/tools/ocrTool';
import { CaptureHistoryActionType, CaptureHistoryController } from './components/captureHistory';
import { AntdContext } from '@/components/globalLayoutExtra';
import { appError } from '@/utils/log';
import { NonDeletedExcalidrawElement } from '@mg-chao/excalidraw/element/types';

const DrawCacheLayer = dynamic(
    async () => (await import('./components/drawCacheLayer')).DrawCacheLayer,
    {
        ssr: false,
    },
);

enum DrawPageState {
    /** 初始化状态 */
    Init = 'init',
    /** 激活状态 */
    Active = 'active',
    /** 等待释放状态 */
    WaitRelease = 'wait-release',
    /** 释放状态 */
    Release = 'release',
}

const DrawPageCore: React.FC<{
    getFixedContentAction: () => FixedContentActionType | undefined;
}> = ({ getFixedContentAction }) => {
    const { message } = useContext(AntdContext);
    const intl = useIntl();

    const appWindowRef = useRef<AppWindow>(undefined as unknown as AppWindow);
    useEffect(() => {
        appWindowRef.current = getCurrentWindow();
    }, []);

    // 截图原始数据
    const imageBufferRef = useRef<ImageBuffer | undefined>(undefined);
    const captureBoundingBoxInfoRef = useRef<CaptureBoundingBoxInfo | undefined>(undefined);
    const imageBlobUrlRef = useRef<string | undefined>(undefined);
    const { addListener, removeListener } = useContext(EventListenerContext);

    // 层级
    const drawLayerWrapRef = useRef<HTMLDivElement>(null);
    const layerContainerRef = useRef<HTMLDivElement>(null);
    const drawLayerActionRef = useRef<DrawLayerActionType | undefined>(undefined);
    const drawCacheLayerActionRef = useRef<DrawCacheLayerActionType | undefined>(undefined);
    const selectLayerActionRef = useRef<SelectLayerActionType | undefined>(undefined);
    const drawToolbarActionRef = useRef<DrawToolbarActionType | undefined>(undefined);
    const colorPickerActionRef = useRef<ColorPickerActionType | undefined>(undefined);
    const captureHistoryActionRef = useRef<CaptureHistoryActionType | undefined>(undefined);
    const ocrBlocksActionRef = useRef<OcrBlocksActionType | undefined>(undefined);

    // 状态
    const drawPageStateRef = useRef<DrawPageState>(DrawPageState.Init);
    const mousePositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const [getScreenshotType, setScreenshotType, resetScreenshotType] = useStateSubscriber(
        ScreenshotTypePublisher,
        undefined,
    );
    const [getCaptureStep, setCaptureStep, resetCaptureStep] = useStateSubscriber(
        CaptureStepPublisher,
        undefined,
    );
    const [getDrawState, , resetDrawState] = useStateSubscriber(
        DrawStatePublisher,
        useCallback((drawState: DrawState) => {
            if (drawState === DrawState.Text) {
                setCurrentWindowAlwaysOnTop(true);
            } else {
                setCurrentWindowAlwaysOnTop(false);
            }
        }, []),
    );
    const [, setCaptureLoading] = useStateSubscriber(CaptureLoadingPublisher, undefined);
    const [getCaptureEvent, setCaptureEvent] = useStateSubscriber(CaptureEventPublisher, undefined);
    const onCaptureLoad = useCallback<BaseLayerEventActionType['onCaptureLoad']>(
        async (
            imageSrc: string,
            imageBuffer: ImageBuffer,
            captureBoundingBoxInfo: CaptureBoundingBoxInfo,
        ) => {
            await Promise.all([
                drawLayerActionRef.current?.onCaptureLoad(
                    imageSrc,
                    imageBuffer,
                    captureBoundingBoxInfo,
                ),
            ]);

            setCaptureEvent({
                event: CaptureEvent.onCaptureLoad,
                params: [imageSrc, imageBuffer, captureBoundingBoxInfo],
            });
        },
        [setCaptureEvent],
    );
    const capturingRef = useRef(false);
    const circleCursorRef = useRef<HTMLDivElement>(null);

    const { history } = useContext(HistoryContext);

    const handleLayerSwitch = useCallback((layer: CanvasLayer) => {
        switchLayer(layer, drawLayerActionRef.current, selectLayerActionRef.current);
    }, []);
    const onCaptureStepDrawStateChange = useCallback(() => {
        const captureStep = getCaptureStep();
        const drawState = getDrawState();

        if (captureStep === CaptureStep.Select) {
            handleLayerSwitch(CanvasLayer.Select);
            return;
        } else if (captureStep === CaptureStep.Draw) {
            if (drawState === DrawState.Idle) {
                handleLayerSwitch(CanvasLayer.Select);
                return;
            }

            handleLayerSwitch(CanvasLayer.Draw);
            return;
        }

        handleLayerSwitch(CanvasLayer.Select);
    }, [getCaptureStep, getDrawState, handleLayerSwitch]);
    const onCaptureStepDrawStateChangeDebounce = useMemo(() => {
        return debounce(onCaptureStepDrawStateChange, 0);
    }, [onCaptureStepDrawStateChange]);
    useStateSubscriber(CaptureStepPublisher, onCaptureStepDrawStateChangeDebounce);
    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState) => {
                onCaptureStepDrawStateChangeDebounce();

                if (!drawLayerWrapRef.current) {
                    return;
                }

                if (drawState === DrawState.ScrollScreenshot) {
                    drawLayerWrapRef.current.style.opacity = '0';
                } else {
                    drawLayerWrapRef.current.style.opacity = '1';
                }
            },
            [onCaptureStepDrawStateChangeDebounce],
        ),
    );

    /** 截图准备 */
    const readyCapture = useCallback(
        async (imageBuffer: ImageBuffer, captureBoundingBoxInfo: CaptureBoundingBoxInfo) => {
            setCaptureLoading(true);

            if (imageBlobUrlRef.current) {
                const tempUrl = imageBlobUrlRef.current;
                // 延迟释放 URL，提速
                setTimeout(() => {
                    URL.revokeObjectURL(tempUrl);
                }, 0);
            }

            imageBlobUrlRef.current = URL.createObjectURL(new Blob([imageBuffer.data]));

            setCaptureEvent({
                event: CaptureEvent.onCaptureImageBufferReady,
                params: {
                    imageBuffer,
                },
            });

            mousePositionRef.current = new MousePosition(
                Math.floor(captureBoundingBoxInfo.mousePosition.mouseX / window.devicePixelRatio),
                Math.floor(captureBoundingBoxInfo.mousePosition.mouseY / window.devicePixelRatio),
            );

            await Promise.all([
                drawLayerActionRef.current?.onCaptureReady(
                    imageBlobUrlRef.current,
                    imageBuffer,
                    captureBoundingBoxInfo,
                ),
                drawCacheLayerActionRef.current?.onCaptureReady(),
            ]);
            setCaptureEvent({
                event: CaptureEvent.onCaptureReady,
                params: [imageBlobUrlRef.current, imageBuffer, captureBoundingBoxInfo],
            });
            setCaptureLoading(false);

            onCaptureLoad(imageBlobUrlRef.current, imageBuffer, captureBoundingBoxInfo);
        },
        [onCaptureLoad, setCaptureLoading, setCaptureEvent],
    );

    /** 显示截图窗口 */
    const showWindow = useCallback(
        async ({ min_x, min_y, max_x, max_y }: ElementRect) => {
            const appWindow = appWindowRef.current;

            await Promise.all([
                setWindowRect(appWindow, { min_x, min_y, max_x, max_y }),
                appWindow.setAlwaysOnTop(true),
            ]);

            if (layerContainerRef.current) {
                const documentWidth = (max_x - min_x) / window.devicePixelRatio;
                const documentHeight = (max_y - min_y) / window.devicePixelRatio;

                layerContainerRef.current.style.width = `${documentWidth}px`;
                layerContainerRef.current.style.height = `${documentHeight}px`;
            }

            await showCurrentWindow();
            if (
                process.env.NODE_ENV === 'development' &&
                getScreenshotType() !== ScreenshotType.TopWindow
            ) {
                await appWindow.setAlwaysOnTop(false);
            }

            setCurrentWindowAlwaysOnTop(false);

            // 监听键盘
            listenKeyStart().catch((error) => {
                appError('[DrawPageCore] listenKeyStart error', error);
            });

            setDrawWindowStyle();
        },
        [getScreenshotType],
    );

    const hideWindow = useCallback(async () => {
        await appWindowRef.current.hide();
        // await Promise.all([
        //     appWindowRef.current.show(),
        //     appWindowRef.current.setSize(new PhysicalSize(100, 100)),
        // ]);
    }, []);

    const releasePage = useMemo(() => {
        return debounce(async () => {
            if (drawPageStateRef.current !== DrawPageState.WaitRelease) {
                return;
            }

            drawPageStateRef.current = DrawPageState.Release;
            await Promise.all([
                createDrawWindow(),
                // 隔一段时间释放，防止释放中途用户唤起
                new Promise((resolve) => {
                    setTimeout(resolve, 1000 * 8);
                }).then(() => {
                    appWindowRef.current.close();
                }),
            ]);
        }, 1000 * 16);
    }, []);

    const finishCapture = useCallback<DrawContextType['finishCapture']>(
        async (clearScrollScreenshot: boolean = true) => {
            // 停止监听键盘
            listenKeyStop().catch((error) => {
                appError('[DrawPageCore] listenKeyStop error', error);
            });

            drawPageStateRef.current = DrawPageState.WaitRelease;
            releasePage();

            if (clearScrollScreenshot) {
                scrollScreenshotClear();
            }

            window.getSelection()?.removeAllRanges();
            await Promise.all([
                drawLayerActionRef.current?.onCaptureFinish(),
                selectLayerActionRef.current?.onCaptureFinish(),
                drawCacheLayerActionRef.current?.onCaptureFinish(),
            ]);

            setCaptureEvent({
                event: CaptureEvent.onCaptureFinish,
            });
            imageBufferRef.current = undefined;
            resetCaptureStep();
            resetDrawState();
            resetScreenshotType();
            drawToolbarActionRef.current?.setEnable(false);
            capturingRef.current = false;
            history.clear();

            // 等待 1 帧，确保截图窗口内的元素均隐藏完成
            setTimeout(() => {
                hideWindow();
            }, 17);
        },
        [
            hideWindow,
            history,
            releasePage,
            resetCaptureStep,
            resetDrawState,
            resetScreenshotType,
            setCaptureEvent,
        ],
    );

    const initCaptureBoundingBoxInfoAndShowWindow = useCallback(async () => {
        const [captureBoundingBox, mousePosition] = await Promise.all([
            getMonitorsBoundingBox(),
            getMousePosition().catch((error) => {
                appError('[DrawPageCore] getMousePosition error', error);
                message.error(<FormattedMessage id="draw.getMousePositionError" />);
                return [0, 0];
            }),
        ]);

        const rTree = new Flatbush(captureBoundingBox.monitor_rect_list.length);
        captureBoundingBox.monitor_rect_list.forEach((rect) => {
            rTree.add(rect.min_x, rect.min_y, rect.max_x, rect.max_y);
        });
        rTree.finish();

        captureBoundingBoxInfoRef.current = new CaptureBoundingBoxInfo(
            captureBoundingBox.rect,
            captureBoundingBox.monitor_rect_list,
            new MousePosition(mousePosition[0], mousePosition[1]),
        );

        await Promise.all([
            await showWindow(captureBoundingBoxInfoRef.current.rect),
            selectLayerActionRef.current?.onCaptureBoundingBoxInfoReady(
                captureBoundingBoxInfoRef.current!,
            ),
            drawLayerActionRef.current?.onCaptureBoundingBoxInfoReady(
                captureBoundingBoxInfoRef.current!,
            ),
        ]);
    }, [message, showWindow]);

    /** 执行截图 */
    const excuteScreenshot = useCallback(
        async (excuteScreenshotType: ScreenshotType) => {
            capturingRef.current = true;
            drawToolbarActionRef.current?.setEnable(false);

            const initCaptureBoundingBoxInfoPromise = initCaptureBoundingBoxInfoAndShowWindow();
            const captureAllMonitorsPromise = captureAllMonitors().catch((error) => {
                appError('[DrawPageCore] captureAllMonitors error', error);
                return undefined;
            });

            setScreenshotType(excuteScreenshotType);
            const layerOnExecuteScreenshotPromise = Promise.all([
                drawLayerActionRef.current?.onExecuteScreenshot(),
                selectLayerActionRef.current?.onExecuteScreenshot(),
            ]);
            setCaptureEvent({
                event: CaptureEvent.onExecuteScreenshot,
            });

            let imageBuffer: ImageBuffer | undefined;
            try {
                imageBuffer = await captureAllMonitorsPromise;
            } catch {
                imageBuffer = undefined;
            }
            await initCaptureBoundingBoxInfoPromise;

            // 如果截图失败了，等窗口显示后，结束截图
            if (!imageBuffer) {
                sendErrorMessage(intl.formatMessage({ id: 'draw.captureError' }));

                finishCapture();
                return;
            }

            imageBufferRef.current = imageBuffer;

            // 防止用户提前退出报错
            if (getCaptureEvent()?.event !== CaptureEvent.onExecuteScreenshot) {
                return;
            }

            try {
                // 因为窗口是空的，所以窗口显示和图片显示先后顺序倒无所谓
                await Promise.all([
                    readyCapture(imageBufferRef.current, captureBoundingBoxInfoRef.current!),
                    layerOnExecuteScreenshotPromise,
                ]);
            } catch (error) {
                // 防止用户提前退出报错
                if (getCaptureEvent()?.event !== CaptureEvent.onExecuteScreenshot) {
                    return;
                }

                throw error;
            }
        },
        [
            setScreenshotType,
            setCaptureEvent,
            initCaptureBoundingBoxInfoAndShowWindow,
            getCaptureEvent,
            intl,
            finishCapture,
            readyCapture,
        ],
    );

    const saveCaptureHistory = useCallback(async () => {
        const imageBuffer = imageBufferRef.current;

        updateAppSettings(
            AppSettingsGroup.Cache,
            {
                prevSelectRect: selectLayerActionRef.current?.getSelectRect(),
            },
            false,
            true,
            false,
            true,
            false,
        );

        await captureHistoryActionRef.current?.saveCurrentCapture(imageBuffer);
    }, [updateAppSettings]);

    const onSave = useCallback(
        async (fastSave: boolean = false) => {
            saveCaptureHistory();

            if (getDrawState() === DrawState.ScrollScreenshot) {
                const imagePath =
                    (await getImagePathFromSettings(
                        fastSave ? getAppSettings() : undefined,
                        'fast',
                    )) ?? (await showImageDialog(getAppSettings()));

                if (!imagePath) {
                    return;
                }

                if (!fastSave) {
                    updateAppSettings(
                        AppSettingsGroup.Cache,
                        {
                            prevImageFormat: imagePath.imageFormat,
                        },
                        false,
                        true,
                        false,
                        true,
                        false,
                    );
                }

                scrollScreenshotSaveToFile(imagePath.filePath).then(() => {
                    scrollScreenshotClear();
                });
                finishCapture();
                return;
            }

            if (
                !selectLayerActionRef.current ||
                !drawLayerActionRef.current ||
                !drawCacheLayerActionRef.current
            ) {
                return;
            }

            const imageCanvas = await getCanvas(
                selectLayerActionRef.current.getSelectRectParams(),
                drawLayerActionRef.current,
                drawCacheLayerActionRef.current,
            );

            saveToFile(
                getAppSettings(),
                imageCanvas,
                async (filePath: string) => {
                    if (!fastSave) {
                        updateAppSettings(
                            AppSettingsGroup.Cache,
                            {
                                prevImageFormat: getImageFormat(filePath),
                            },
                            false,
                            true,
                            false,
                            true,
                            false,
                        );
                    }

                    finishCapture();
                },
                getAppSettings()[AppSettingsGroup.Cache].prevImageFormat,
                fastSave ? await getImagePathFromSettings(getAppSettings(), 'fast') : undefined,
            );
        },
        [finishCapture, getAppSettings, getDrawState, saveCaptureHistory, updateAppSettings],
    );

    const onFixed = useCallback(async () => {
        // 停止监听键盘
        listenKeyStop();

        if (getDrawState() === DrawState.ScrollScreenshot) {
            createFixedContentWindow(true);
            finishCapture(false);
            return;
        }

        const fixedContentAction = getFixedContentAction();

        if (
            !layerContainerRef.current ||
            !selectLayerActionRef.current ||
            !captureBoundingBoxInfoRef.current ||
            !drawLayerActionRef.current ||
            !drawCacheLayerActionRef.current ||
            !fixedContentAction ||
            !ocrBlocksActionRef.current
        ) {
            return;
        }

        // 非滚动截图时保存截屏历史
        saveCaptureHistory();

        await fixedToScreen(
            captureBoundingBoxInfoRef.current,
            appWindowRef.current,
            layerContainerRef.current,
            selectLayerActionRef.current,
            drawLayerActionRef.current,
            drawCacheLayerActionRef.current,
            fixedContentAction,
            setCaptureStep,
            // 如果当前是 OCR 识别状态，则使用已有的 OCR 结果
            isOcrTool(getDrawState())
                ? ocrBlocksActionRef.current?.getOcrResultAction()?.getOcrResult()
                : undefined,
        );

        switchLayer(undefined, drawLayerActionRef.current, selectLayerActionRef.current);

        imageBufferRef.current = undefined;
    }, [finishCapture, getDrawState, getFixedContentAction, saveCaptureHistory, setCaptureStep]);

    const onTopWindow = useCallback(async () => {
        const windowId = selectLayerActionRef.current?.getWindowId();

        if (windowId) {
            await switchAlwaysOnTop(windowId);
        }

        await finishCapture();
    }, [finishCapture]);

    const onOcrDetect = useCallback(async () => {
        if (
            !captureBoundingBoxInfoRef.current ||
            !selectLayerActionRef.current ||
            !drawLayerActionRef.current ||
            !drawCacheLayerActionRef.current ||
            !ocrBlocksActionRef.current
        ) {
            return;
        }

        handleOcrDetect(
            captureBoundingBoxInfoRef.current,
            selectLayerActionRef.current,
            drawLayerActionRef.current,
            drawCacheLayerActionRef.current,
            ocrBlocksActionRef.current,
            true,
        );
    }, []);

    const onCopyToClipboard = useCallback(async () => {
        saveCaptureHistory();

        const enableAutoSave = getAppSettings()[AppSettingsGroup.FunctionScreenshot].autoSaveOnCopy;

        if (getDrawState() === DrawState.ScrollScreenshot) {
            const filePath = (await getImagePathFromSettings(getAppSettings(), 'auto'))?.filePath;
            Promise.all([
                scrollScreenshotSaveToClipboard(),
                enableAutoSave && filePath
                    ? scrollScreenshotSaveToFile(filePath)
                    : Promise.resolve(),
            ]).then(() => {
                scrollScreenshotClear();
            });
            finishCapture();
            return;
        }

        const selectedText = window.getSelection()?.toString().trim();

        if (
            selectedText &&
            (isOcrTool(getDrawState()) || getDrawState() === DrawState.ScanQrcode)
        ) {
            writeTextToClipboard(selectedText);
            finishCapture();
            return;
        } else if (
            isOcrTool(getDrawState()) &&
            getAppSettings()[AppSettingsGroup.FunctionScreenshot].ocrCopyText
        ) {
            const ocrResult = ocrBlocksActionRef.current?.getOcrResultAction()?.getOcrResult();
            writeTextToClipboard(ocrResult ? covertOcrResultToText(ocrResult.result) : '');
            finishCapture();
            return;
        } else {
            if (
                !selectLayerActionRef.current ||
                !drawLayerActionRef.current ||
                !drawCacheLayerActionRef.current
            ) {
                return;
            }

            const imageCanvas = await getCanvas(
                selectLayerActionRef.current.getSelectRectParams(),
                drawLayerActionRef.current,
                drawCacheLayerActionRef.current,
            );

            finishCapture();

            copyToClipboard(imageCanvas);

            if (enableAutoSave) {
                await saveToFile(
                    getAppSettings(),
                    imageCanvas,
                    undefined,
                    undefined,
                    await getImagePathFromSettings(getAppSettings(), 'auto'),
                );
            }
        }
    }, [finishCapture, getAppSettings, getDrawState, saveCaptureHistory]);

    const releaseExecuteScreenshotTimerRef = useRef<
        | {
              timer: NodeJS.Timeout | undefined;
              type: ScreenshotType;
          }
        | undefined
    >(undefined);

    useEffect(() => {
        // 监听截图命令
        const listenerId = addListener('execute-screenshot', (args) => {
            const payload = (args as { payload: { type: ScreenshotType } }).payload;

            if (capturingRef.current) {
                return;
            }

            if (drawPageStateRef.current === DrawPageState.Init) {
                return;
            } else if (drawPageStateRef.current === DrawPageState.Release) {
                // 这时候可能窗口还在加载中，每隔一段时间触发下截图
                if (releaseExecuteScreenshotTimerRef.current?.timer) {
                    clearInterval(releaseExecuteScreenshotTimerRef.current.timer);
                }
                releaseExecuteScreenshotTimerRef.current = {
                    timer: setInterval(() => {
                        executeScreenshotFunc(payload.type);
                    }, 128),
                    type: payload.type,
                };

                return;
            } else if (drawPageStateRef.current === DrawPageState.WaitRelease) {
                // 重置为激活状态
                drawPageStateRef.current = DrawPageState.Active;
            }

            excuteScreenshot(payload.type);
        });

        const finishListenerId = addListener('finish-screenshot', () => {
            finishCapture();
        });

        const releaseListenerId = addListener('release-draw-page', (args) => {
            const payload = (args as { payload: { force: boolean } }).payload;

            if (!payload.force) {
                if (drawPageStateRef.current !== DrawPageState.Release) {
                    return;
                }

                if (releaseExecuteScreenshotTimerRef.current?.timer) {
                    clearInterval(releaseExecuteScreenshotTimerRef.current.timer);
                    executeScreenshotFunc(releaseExecuteScreenshotTimerRef.current.type);
                }
            }

            getCurrentWindow().close();
        });

        return () => {
            removeListener(listenerId);
            removeListener(finishListenerId);
            removeListener(releaseListenerId);
        };
    }, [addListener, excuteScreenshot, removeListener, finishCapture]);

    // 默认隐藏
    useEffect(() => {
        hideWindow();
    }, [hideWindow]);

    const drawContextValue = useMemo<DrawContextType>(() => {
        return {
            finishCapture,
            drawLayerActionRef,
            selectLayerActionRef,
            imageBufferRef,
            drawToolbarActionRef,
            mousePositionRef,
            circleCursorRef,
            drawCacheLayerActionRef,
            ocrBlocksActionRef,
            colorPickerActionRef,
            captureBoundingBoxInfoRef,
            captureHistoryActionRef,
        };
    }, [finishCapture]);

    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            mousePositionRef.current = new MousePosition(e.clientX, e.clientY);
        };

        document.addEventListener('mousemove', handleMouseMove);

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
        };
    }, [onCopyToClipboard]);

    useEffect(() => {
        document.oncopy = function () {
            if (getCaptureStep() === CaptureStep.Fixed || isOcrTool(getDrawState())) {
                return true;
            }

            return false;
        };
    }, [getCaptureStep, getDrawState, onCopyToClipboard]);

    const latestExcalidrawNewElementRef = useRef<
        | {
              id: string;
              created: number;
          }
        | {
              editingTextElement: NonDeletedExcalidrawElement;
          }
        | undefined
    >(undefined);
    const unsetLatestExcalidrawNewElement = useMemo(() => {
        return debounce(() => {
            latestExcalidrawNewElementRef.current = undefined;
        }, 512);
    }, []);
    const onDoubleClickFirstClick = useCallback(() => {
        // 判断 excalidraw 是否在绘制中
        const excalidrawAPI = drawCacheLayerActionRef.current?.getExcalidrawAPI();
        const sceneElements = excalidrawAPI?.getSceneElements();
        const currentAppState = excalidrawAPI?.getAppState();
        const newElement = currentAppState?.newElement;
        const editingTextElement = currentAppState?.editingTextElement;
        const selectedElements = sceneElements?.filter(
            (element) => currentAppState?.selectedElementIds[element.id],
        );
        const selectedGroupIds = Object.keys(currentAppState?.selectedGroupIds ?? {}).filter(
            (id) => currentAppState?.selectedGroupIds[id],
        );

        if (selectedElements && selectedGroupIds && selectedGroupIds.length === 1) {
            latestExcalidrawNewElementRef.current = {
                editingTextElement: selectedElements[0],
            };
        } else if (selectedElements?.length === 1 && selectedElements[0].type === 'text') {
            latestExcalidrawNewElementRef.current = {
                editingTextElement: selectedElements[0],
            };
        } else if (editingTextElement) {
            latestExcalidrawNewElementRef.current = {
                editingTextElement: editingTextElement,
            };
        } else if (newElement && 'updated' in newElement) {
            let created = newElement.updated;
            if (
                latestExcalidrawNewElementRef.current &&
                'id' in latestExcalidrawNewElementRef.current &&
                latestExcalidrawNewElementRef.current.id === newElement.id
            ) {
                created = latestExcalidrawNewElementRef.current.created ?? created;
            }

            latestExcalidrawNewElementRef.current = {
                id: newElement.id,
                created: created,
            };
        } else {
            unsetLatestExcalidrawNewElement();
        }
    }, [unsetLatestExcalidrawNewElement]);
    const onDoubleClick = useCallback<React.MouseEventHandler<HTMLDivElement>>(
        (e) => {
            if (
                e.button === 0 &&
                // 如果存在创建时间大于512ms的在编辑中的元素，则认为是对箭头的双击
                !(
                    (latestExcalidrawNewElementRef.current &&
                        'created' in latestExcalidrawNewElementRef.current &&
                        latestExcalidrawNewElementRef.current.created < Date.now() - 512) ||
                    (latestExcalidrawNewElementRef.current &&
                        'editingTextElement' in latestExcalidrawNewElementRef.current)
                )
            ) {
                onCopyToClipboard();
            }
        },
        [onCopyToClipboard],
    );

    useEffect(() => {
        drawLayerActionRef.current?.initCanvas(false).then(() => {
            drawPageStateRef.current = DrawPageState.Active;
            releaseDrawPage();
        });
    }, []);

    return (
        <DrawContext.Provider value={drawContextValue}>
            <div
                className={styles.layerContainer}
                ref={layerContainerRef}
                onDoubleClick={onDoubleClick}
                onClick={onDoubleClickFirstClick}
            >
                <CaptureHistoryController actionRef={captureHistoryActionRef} />

                <ExtraTool finishCapture={finishCapture} />

                <OcrBlocks actionRef={ocrBlocksActionRef} finishCapture={finishCapture} />

                <div className={styles.drawLayerWrap} ref={drawLayerWrapRef}>
                    <DrawLayer actionRef={drawLayerActionRef} />
                    <DrawCacheLayer actionRef={drawCacheLayerActionRef} />
                </div>
                <SelectLayer actionRef={selectLayerActionRef} />
                <DrawToolbar
                    actionRef={drawToolbarActionRef}
                    onCancel={finishCapture}
                    onSave={onSave}
                    onFixed={onFixed}
                    onCopyToClipboard={onCopyToClipboard}
                    onOcrDetect={onOcrDetect}
                    onTopWindow={onTopWindow}
                />
                <ColorPicker
                    onCopyColor={() => {
                        finishCapture();
                    }}
                    actionRef={colorPickerActionRef}
                />
                <StatusBar />

                <div
                    ref={circleCursorRef}
                    className={styles.drawToolbarCursor}
                    style={{ zIndex: zIndexs.Draw_Cursor }}
                />
            </div>
        </DrawContext.Provider>
    );
};

const DrawPageContent = React.memo(
    withCanvasHistory(
        withStatePublisher(
            DrawPageCore,
            CaptureStepPublisher,
            DrawStatePublisher,
            CaptureLoadingPublisher,
            EnableKeyEventPublisher,
            ExcalidrawEventPublisher,
            CaptureEventPublisher,
            ExcalidrawOnHandleEraserPublisher,
            ScreenshotTypePublisher,
            DrawEventPublisher,
            DrawToolbarStatePublisher,
        ),
    ),
);

const DrawPage = () => {
    const [isFixed, setIsFixed] = useState(false);
    const fixedContentActionRef = useRef<FixedContentActionType | undefined>(undefined);

    const getFixedContentAction = useCallback(() => {
        return fixedContentActionRef.current;
    }, []);

    return (
        <>
            {!isFixed && <DrawPageContent getFixedContentAction={getFixedContentAction} />}
            <div>
                <FixedContentCore
                    actionRef={fixedContentActionRef}
                    onDrawLoad={() => {
                        setIsFixed(true);
                    }}
                    disabled={!isFixed}
                />
            </div>
        </>
    );
};

export default DrawPage;
