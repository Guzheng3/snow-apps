'use client';

import React, { useEffect } from 'react';
import { useCallback, useImperativeHandle, useMemo, useRef } from 'react';
import { Excalidraw } from '@mg-chao/excalidraw';
import {
    ExcalidrawInitialDataState,
    ExcalidrawImperativeAPI,
    ExcalidrawActionType,
    ExcalidrawPropsCustomOptions,
    AppState,
    ExcalidrawProps,
} from '@mg-chao/excalidraw/types';
import '@mg-chao/excalidraw/index.css';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { ExcalidrawKeyEventHandler } from './components/excalidrawKeyEventHandler';
import {
    convertLocalToLocalCode,
    DrawCoreActionType,
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventCallbackPublisher,
    ExcalidrawEventCallbackType,
    ExcalidrawEventPublisher,
    ExcalidrawKeyEventPublisher,
    ExcalidrawOnHandleEraserPublisher,
} from './extra';
import { useIntl } from 'react-intl';
import { theme } from 'antd';
import { layoutRenders } from './excalidrawRenders';
import { pickerRenders } from './excalidrawRenders';
import { ElementRect } from '@/commands';
import { ExcalidrawAppStateStore } from '@/utils/appStore';
import { debounce } from 'es-toolkit';
import { useHistory } from './components/historyContext';
import {
    SerialNumberContextProvider,
    SerialNumberTool,
} from '@/app/fullScreenDraw/components/drawCore/components/serialNumberTool';
import { ExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { usePlatform } from '@/hooks/usePlatform';

const strokeWidthList = [1, 2, 4];
const fontSizeList = [16, 20, 28, 36];

// 在 DrawCacheLayerCore 组件外部添加一个辅助函数
const getNextValueInList = <T,>(currentValue: T, valueList: T[], isIncrease: boolean): T => {
    const currentIndex = valueList.indexOf(currentValue);
    if (currentIndex !== -1) {
        if (isIncrease) {
            // 选择下一个值（循环到开头）
            const nextIndex = (currentIndex + 1) % valueList.length;
            return valueList[nextIndex];
        } else {
            // 选择上一个值（循环到结尾）
            const prevIndex = (currentIndex - 1 + valueList.length) % valueList.length;
            return valueList[prevIndex];
        }
    } else {
        // 如果当前值不在列表中
        return isIncrease ? valueList[0] : valueList[valueList.length - 1];
    }
};

const storageKey = 'global';
const DrawCoreComponent: React.FC<{
    actionRef: React.RefObject<DrawCoreActionType | undefined>;
    zIndex: number;
    layoutMenuZIndex: number;
    excalidrawCustomOptions?: NonNullable<ExcalidrawPropsCustomOptions>;
    onLoad?: () => void;
    appStateStorageKey?: string;
}> = ({
    actionRef,
    zIndex,
    layoutMenuZIndex,
    excalidrawCustomOptions: excalidrawCustomOptionsProp,
    onLoad,
    appStateStorageKey = storageKey,
}) => {
    const { token } = theme.useToken();
    const intl = useIntl();

    const { history } = useHistory();

    const initialData = useMemo<ExcalidrawInitialDataState>(() => {
        return {
            appState: { viewBackgroundColor: '#00000000' },
        };
    }, []);

    const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const [, setExcalidrawEvent] = useStateSubscriber(ExcalidrawEventPublisher, undefined);
    const [, setExcalidrawOnHandleEraserEvent] = useStateSubscriber(
        ExcalidrawOnHandleEraserPublisher,
        undefined,
    );
    const [getExcalidrawKeyEvent] = useStateSubscriber(ExcalidrawKeyEventPublisher, undefined);
    const [, setExcalidrawEventCallback] = useStateSubscriber(
        ExcalidrawEventCallbackPublisher,
        undefined,
    );
    const drawCacheLayerElementRef = useRef<HTMLDivElement>(null);
    const excalidrawAPIRef = useRef<ExcalidrawImperativeAPI>(undefined);
    const excalidrawAPI = useCallback((api: ExcalidrawImperativeAPI) => {
        excalidrawAPIRef.current = api;
    }, []);
    const excalidrawActionRef = useRef<ExcalidrawActionType>(undefined);

    const updateScene = useCallback<DrawCoreActionType['updateScene']>((...args) => {
        excalidrawAPIRef.current?.updateScene(...args);
    }, []);

    const getCanvas = useCallback<DrawCoreActionType['getCanvas']>(() => {
        const canvas = document.getElementById(
            'excalidraw__content-canvas',
        ) as HTMLCanvasElement | null;
        return canvas;
    }, []);

    const getCanvasContext = useCallback<DrawCoreActionType['getCanvasContext']>(() => {
        const canvas = getCanvas();
        if (!canvas) {
            return;
        }

        return canvas.getContext('2d');
    }, [getCanvas]);

    const getImageData = useCallback<DrawCoreActionType['getImageData']>(
        async (selectRect: ElementRect) => {
            const canvasContext = getCanvasContext();
            if (!canvasContext) {
                return;
            }

            return canvasContext.getImageData(
                selectRect.min_x,
                selectRect.min_y,
                selectRect.max_x - selectRect.min_x,
                selectRect.max_y - selectRect.min_y,
            );
        },
        [getCanvasContext],
    );

    const excalidrawAppStateStoreRef = useRef<ExcalidrawAppStateStore>(undefined);
    const excalidrawAppStateStoreValue = useRef<
        | {
              appState: Partial<AppState>;
          }
        | undefined
    >(undefined);
    useEffect(() => {
        if (excalidrawAppStateStoreRef.current) {
            return;
        }

        excalidrawAppStateStoreRef.current = new ExcalidrawAppStateStore();
        excalidrawAppStateStoreRef.current.init().then(() => {
            excalidrawAppStateStoreRef.current!.get(appStateStorageKey).then((value) => {
                if (value) {
                    if (excalidrawAPIRef.current) {
                        // 未初始化 setstate 报错，未发现具体原因，延迟处理下
                        setTimeout(() => {
                            excalidrawAPIRef.current!.updateScene({
                                appState: {
                                    ...(value.appState as AppState),
                                    viewBackgroundColor: '#00000000',
                                },
                            });
                        }, 0);
                    } else {
                        excalidrawAppStateStoreValue.current = {
                            appState: {
                                ...(value.appState as AppState),
                                viewBackgroundColor: '#00000000',
                            },
                        };
                    }
                }
            });
        });
    }, [appStateStorageKey]);

    const handleWheel = useCallback(
        (
            event: WheelEvent | React.WheelEvent<HTMLDivElement | HTMLCanvasElement>,
            zoomAction?: () => void,
        ) => {
            if (!excalidrawAPIRef.current) {
                return;
            }

            if ((event.metaKey || event.ctrlKey) && zoomAction) {
                zoomAction();
                return;
            }

            const appState = excalidrawAPIRef.current.getAppState();
            if (!appState) {
                return;
            }

            const isIncrease = event.deltaY < 0;

            // 判断是否有选中单个元素
            // 只对单个元素的情况进行处理
            const sceneElements = excalidrawAPIRef.current?.getSceneElements() ?? [];
            let selectedElement: ExcalidrawElement | undefined = undefined;
            const selectedElementIds = appState.selectedElementIds;
            if (Object.keys(selectedElementIds).length === 1) {
                selectedElement = sceneElements.find((item) => selectedElementIds[item.id]);
            }

            let changeTargetProp:
                | {
                      blur: number;
                  }
                | {
                      fontSize: number;
                  }
                | {
                      strokeWidth: number;
                  };
            if (getDrawState() === DrawState.Blur || selectedElement?.type === 'blur') {
                const currentBlur =
                    selectedElement && 'blur' in selectedElement
                        ? selectedElement.blur
                        : appState.currentItemBlur;
                const targetBlur = Math.max(
                    Math.min(currentBlur + (isIncrease ? 1 : -1) * 10, 100),
                    0,
                );

                changeTargetProp = {
                    blur: targetBlur,
                };
            } else if (
                getDrawState() === DrawState.Text ||
                getDrawState() === DrawState.SerialNumber ||
                selectedElement?.type === 'text'
            ) {
                const currentFontSize =
                    selectedElement && 'fontSize' in selectedElement
                        ? selectedElement.fontSize
                        : appState.currentItemFontSize;

                const targetFontSize = getNextValueInList(
                    currentFontSize,
                    fontSizeList,
                    isIncrease,
                );

                changeTargetProp = {
                    fontSize: targetFontSize,
                };
            } else {
                const currentStrokeWidth =
                    selectedElement && 'strokeWidth' in selectedElement
                        ? selectedElement.strokeWidth
                        : appState.currentItemStrokeWidth;
                const targetStrokeWidth = getNextValueInList(
                    currentStrokeWidth,
                    strokeWidthList,
                    isIncrease,
                );

                changeTargetProp = {
                    strokeWidth: targetStrokeWidth,
                };
            }

            if ('fontSize' in changeTargetProp) {
                setExcalidrawEventCallback({
                    event: ExcalidrawEventCallbackType.ChangeFontSize,
                    params: {
                        fontSize: changeTargetProp.fontSize,
                        drawState: getDrawState(),
                    },
                });
                setExcalidrawEventCallback(undefined);
            } else if ('strokeWidth' in changeTargetProp) {
                if (selectedElement) {
                    excalidrawAPIRef.current?.updateScene({
                        elements: sceneElements.map((item): ExcalidrawElement => {
                            if (item.id === selectedElement.id) {
                                return {
                                    ...item,
                                    strokeWidth: changeTargetProp.strokeWidth,
                                };
                            }

                            return item;
                        }),
                    });
                } else {
                    excalidrawAPIRef.current?.updateScene({
                        appState: {
                            ...appState,
                            currentItemStrokeWidth: changeTargetProp.strokeWidth,
                        },
                    });
                }
            } else if ('blur' in changeTargetProp) {
                if (selectedElement) {
                    excalidrawAPIRef.current?.updateScene({
                        elements: sceneElements.map((item) => {
                            if (item.id === selectedElement.id) {
                                return {
                                    ...item,
                                    blur: changeTargetProp.blur,
                                };
                            }

                            return item;
                        }),
                    });
                } else {
                    excalidrawAPIRef.current?.updateScene({
                        appState: {
                            ...appState,
                            currentItemBlur: changeTargetProp.blur,
                        },
                    });
                }
            }
        },
        [getDrawState, setExcalidrawEventCallback],
    );

    useImperativeHandle(
        actionRef,
        () => ({
            setActiveTool: (...args) => {
                excalidrawAPIRef.current?.setActiveTool(...args);
            },
            syncActionResult: (...args) => {
                excalidrawActionRef.current?.syncActionResult(...args);
            },
            updateScene,
            getAppState: () => {
                return excalidrawAPIRef.current?.getAppState();
            },
            getImageData,
            getCanvasContext,
            getCanvas,
            getDrawCacheLayerElement: () => drawCacheLayerElementRef.current,
            getExcalidrawAPI: () => excalidrawAPIRef.current,
            finishDraw: () => {
                updateScene({
                    appState: {
                        // 清除在编辑中的元素
                        newElement: null,
                        editingTextElement: null,
                        selectedLinearElement: null,
                        selectionElement: null,
                        selectedElementIds: {},
                        selectedGroupIds: {},
                    },
                    captureUpdate: 'NEVER',
                });
            },
        }),
        [getCanvas, getCanvasContext, getImageData, updateScene],
    );

    const [currentPlatform, currentPlatformRef] = usePlatform();

    // macOS 下 Ctrl、Shift、Command 等键浏览器不会响应，特殊处理下
    const shouldResizeFromCenter = useCallback<
        NonNullable<ExcalidrawPropsCustomOptions['shouldResizeFromCenter']>
    >(
        (event) => {
            if (currentPlatformRef.current === 'macos') {
                return event.altKey;
            }

            return getExcalidrawKeyEvent().resizeFromCenter;
        },
        [currentPlatformRef, getExcalidrawKeyEvent],
    );

    const shouldMaintainAspectRatio = useCallback<
        NonNullable<ExcalidrawPropsCustomOptions['shouldMaintainAspectRatio']>
    >(
        (event) => {
            if (currentPlatformRef.current === 'macos') {
                return event.shiftKey;
            }

            return getExcalidrawKeyEvent().maintainAspectRatio;
        },
        [currentPlatformRef, getExcalidrawKeyEvent],
    );

    const shouldRotateWithDiscreteAngle = useCallback<
        NonNullable<ExcalidrawPropsCustomOptions['shouldRotateWithDiscreteAngle']>
    >(
        (event) => {
            if (currentPlatformRef.current === 'macos') {
                return event.shiftKey;
            }

            return getExcalidrawKeyEvent().rotateWithDiscreteAngle;
        },
        [currentPlatformRef, getExcalidrawKeyEvent],
    );

    const shouldSnapping = useCallback<NonNullable<ExcalidrawPropsCustomOptions['shouldSnapping']>>(
        (event) => {
            if (currentPlatformRef.current === 'macos') {
                return event.metaKey;
            }

            return getExcalidrawKeyEvent().autoAlign;
        },
        [currentPlatformRef, getExcalidrawKeyEvent],
    );

    const onHistoryChange = useCallback<
        NonNullable<ExcalidrawPropsCustomOptions['onHistoryChange']>
    >(
        (_, type) => {
            if (type === 'record') {
                history.pushDrawCacheRecordAction(excalidrawActionRef);
            }
        },
        [history],
    );

    const saveAppState = useCallback(async () => {
        const appState = excalidrawAPIRef.current?.getAppState();
        if (!appState) {
            return;
        }

        if (
            appState.activeTool.type === 'selection' ||
            !excalidrawAppStateStoreRef.current?.inited()
        ) {
            return;
        }

        const storageAppState: Partial<AppState> = {};
        Object.keys(appState)
            .filter((item) => item.startsWith('currentItem'))
            .forEach((item) => {
                const value = appState[item as keyof AppState];
                if (!value) {
                    return;
                }

                storageAppState[item as keyof AppState] = value;
            });

        await excalidrawAppStateStoreRef.current!.set(appStateStorageKey, {
            appState: storageAppState,
        });
    }, [appStateStorageKey]);
    const saveAppStateDebounce = useMemo(() => debounce(saveAppState, 1000), [saveAppState]);

    const getExtraTools = useCallback<
        NonNullable<ExcalidrawPropsCustomOptions['getExtraTools']>
    >(() => {
        if (getDrawState() === DrawState.SerialNumber) {
            return ['serialNumber'];
        }

        return [];
    }, [getDrawState]);

    const onPointerDown = useCallback<NonNullable<ExcalidrawProps['onPointerDown']>>(
        (activeTool, pointerDownState) => {
            setExcalidrawEvent({
                event: 'onPointerDown',
                params: {
                    activeTool,
                    pointerDownState,
                },
            });
            setExcalidrawEvent(undefined);
        },
        [setExcalidrawEvent],
    );
    const onPointerUp = useCallback<NonNullable<ExcalidrawProps['onPointerUp']>>(
        (activeTool, pointerDownState) => {
            setExcalidrawEvent({
                event: 'onPointerUp',
                params: {
                    activeTool,
                    pointerDownState,
                },
            });
            setExcalidrawEvent(undefined);
        },
        [setExcalidrawEvent],
    );

    const excalidrawAPICallback = useCallback<NonNullable<ExcalidrawProps['excalidrawAPI']>>(
        (api) => {
            excalidrawAPI(api);
            onLoad?.();

            if (excalidrawAppStateStoreValue.current) {
                // 未初始化 setstate 报错，未发现具体原因，延迟处理下
                setTimeout(() => {
                    if (!excalidrawAppStateStoreValue.current) {
                        return;
                    }

                    excalidrawAPIRef.current?.updateScene({
                        appState: {
                            ...(excalidrawAppStateStoreValue.current!.appState as AppState),
                        },
                    });
                }, 0);
            }
        },
        [excalidrawAPI, onLoad],
    );

    const excalidrawOnChange = useCallback<NonNullable<ExcalidrawProps['onChange']>>(
        (elements, appState, files) => {
            saveAppStateDebounce();
            setExcalidrawEvent({
                event: 'onChange',
                params: {
                    elements,
                    appState,
                    files,
                },
            });
            setExcalidrawEvent(undefined);
        },
        [saveAppStateDebounce, setExcalidrawEvent],
    );

    const excalidrawCustomOptions = useMemo<NonNullable<ExcalidrawPropsCustomOptions>>(() => {
        return {
            disableKeyEvents: true,
            hideFooter: false,
            onWheel: handleWheel,
            hideMainToolbar: true,
            hideContextMenu: true,
            shouldResizeFromCenter,
            shouldMaintainAspectRatio,
            shouldSnapping,
            getExtraTools,
            shouldRotateWithDiscreteAngle,
            pickerRenders: pickerRenders,
            layoutRenders: layoutRenders,
            onHistoryChange,
            onHandleEraser: (elements) => {
                setExcalidrawOnHandleEraserEvent({
                    elements,
                });
            },
            ...excalidrawCustomOptionsProp,
        };
    }, [
        excalidrawCustomOptionsProp,
        getExtraTools,
        handleWheel,
        onHistoryChange,
        setExcalidrawOnHandleEraserEvent,
        shouldMaintainAspectRatio,
        shouldResizeFromCenter,
        shouldRotateWithDiscreteAngle,
        shouldSnapping,
    ]);

    const excalidrawLangCode = useMemo(() => convertLocalToLocalCode(intl.locale), [intl.locale]);

    return (
        <>
            <SerialNumberContextProvider>
                <div ref={drawCacheLayerElementRef} className="draw-core-layer">
                    <Excalidraw
                        actionRef={excalidrawActionRef}
                        initialData={initialData}
                        handleKeyboardGlobally
                        onPointerDown={onPointerDown}
                        onPointerUp={onPointerUp}
                        excalidrawAPI={excalidrawAPICallback}
                        customOptions={excalidrawCustomOptions}
                        onChange={excalidrawOnChange}
                        langCode={excalidrawLangCode}
                    />
                    {/* macOS 下 Ctrl、Shift、Command 等键浏览器不会响应，特殊处理下 */}
                    {currentPlatform !== 'macos' && <ExcalidrawKeyEventHandler />}

                    <SerialNumberTool />

                    <style jsx>{`
                        .draw-core-layer {
                            top: 0;
                            left: 0;
                            width: 100%;
                            height: 100%;
                        }

                        .draw-core-layer :global(.excalidraw .layer-ui__wrapper) {
                            z-index: unset !important;
                        }

                        .draw-core-layer :global(.excalidraw .layout-menu-render) {
                            --popup-bg-color: ${token.colorBgContainer};
                        }

                        .draw-core-layer :global(.excalidraw .layout-menu-render .picker) {
                            box-shadow: 0 0 3px 0px ${token.colorInfoHover};
                        }

                        .draw-core-layer :global(.excalidraw .layout-menu-render) {
                            position: fixed;
                            z-index: ${layoutMenuZIndex};
                            left: 0;
                            top: 0;
                            box-sizing: border-box;
                            background-color: ${token.colorBgContainer};
                            transition: opacity ${token.motionDurationFast} ${token.motionEaseInOut};
                            box-shadow: 0 0 3px 0px ${token.colorPrimaryHover};
                            color: ${token.colorText};
                            border-radius: ${token.borderRadiusLG}px;
                            animation: slideIn ${token.motionDurationFast} ${token.motionEaseInOut};
                        }

                        @keyframes slideIn {
                            from {
                                opacity: 0;
                            }
                            to {
                                opacity: 1;
                            }
                        }

                        .draw-core-layer :global(.layout-menu-render-drag-button) {
                            text-align: center;
                            margin-top: ${token.marginXS}px;
                            margin-bottom: -${token.marginXS}px;
                        }

                        .draw-core-layer :global(.layout-menu-render-drag-button > span) {
                            transform: rotate(90deg);
                        }

                        .draw-core-layer :global(.Island.App-menu__left) {
                            --text-primary-color: ${token.colorText};

                            background-color: unset !important;
                            box-shadow: unset !important;
                            position: relative !important;
                            padding: ${token.paddingSM}px ${token.paddingSM}px !important;
                        }

                        .draw-core-layer :global(.excalidraw-container-inner) {
                            z-index: ${zIndex};
                            position: fixed;
                        }

                        .draw-core-layer :global(.excalidraw .radio-button-icon) {
                            width: var(--default-icon-size);
                            height: 100%;
                            display: flex;
                            align-items: center;
                        }

                        .draw-core-layer :global(.excalidraw .ant-radio-button-wrapper) {
                            padding-inline: ${0}px;
                        }

                        .draw-core-layer
                            :global(.excalidraw .ant-radio-button-wrapper .radio-button-icon) {
                            padding-inline: ${token.paddingXS}px;
                        }

                        .draw-core-layer :global(.drag-button) {
                            color: ${token.colorTextQuaternary};
                            cursor: move;
                        }

                        .draw-core-layer :global(.draw-toolbar-drag) {
                            font-size: 18px;
                            margin-right: -3px;
                            margin-left: -3px;
                        }

                        .draw-core-layer :global(.excalidraw .scroll-back-to-content) {
                            display: none;
                        }
                    `}</style>
                </div>
            </SerialNumberContextProvider>
        </>
    );
};

export const DrawCore = React.memo(
    withStatePublisher(
        DrawCoreComponent,
        ExcalidrawKeyEventPublisher,
        ExcalidrawEventCallbackPublisher,
    ),
);
