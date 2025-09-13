'use client';

import {
    useCallback,
    useContext,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
    useState,
} from 'react';
import React from 'react';
import { BaseLayerEventActionType } from '../baseLayer';
import {
    ElementRect,
    getElementFromPosition,
    getWindowElements,
    initUiElementsCache,
} from '@/commands';
import {
    AppContext,
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
    AppSettingsTheme,
} from '@/app/contextWrap';
import Flatbush from 'flatbush';
import { useCallbackRender, useCallbackRenderSlow } from '@/hooks/useCallbackRender';
import { TweenAnimation } from '@/utils/tweenAnimation';
import * as TWEEN from '@tweenjs/tween.js';
import {
    convertDragModeToCursor,
    DragMode,
    dragRect,
    drawSelectRect,
    EDGE_DETECTION_TOLERANCE,
    getDragModeFromMousePosition,
    getMaskBackgroundColor,
    limitRect,
    positoinInRect,
    SelectState,
} from './extra';
import { MousePosition } from '@/utils/mousePosition';
import {
    CaptureBoundingBoxInfo,
    CaptureEvent,
    CaptureEventParams,
    CaptureEventPublisher,
    DrawEvent,
    DrawEventParams,
    DrawEventPublisher,
    ScreenshotTypePublisher,
} from '../../extra';
import { CaptureStep, DrawContext } from '../../types';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { CaptureStepPublisher } from '../../extra';
import { ResizeToolbar, ResizeToolbarActionType } from './components/resizeToolbar';
import { ScreenshotType } from '@/functions/screenshot';
import { zIndexs } from '@/utils/zIndex';
import { isHotkeyPressed } from 'react-hotkeys-hook';
import { KeyEventKey } from '../drawToolbar/components/keyEventWrap/extra';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { getPlatform } from '@/utils';
import { useMoveCursor } from '../colorPicker/extra';
import { CaptureHistoryItem } from '@/utils/appStore';
import { useStateRef } from '@/hooks/useStateRef';
import Color from 'color';
import { debounce } from 'es-toolkit';
import { useMonitorRect } from '../statusBar';

export type SelectRectParams = {
    rect: ElementRect;
    radius: number;
    shadowWidth: number;
    shadowColor: string;
};

export type SelectLayerActionType = {
    getSelectRect: () => ElementRect | undefined;
    getSelectRectParams: () => SelectRectParams | undefined;
    switchCaptureHistory: (captureHistory: CaptureHistoryItem | undefined) => void;
    getSelectState: () => SelectState;
    getWindowId: () => number | undefined;
    setEnable: (enable: boolean) => void;
    onExecuteScreenshot: () => Promise<void>;
    onCaptureBoundingBoxInfoReady: (
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
    ) => Promise<void>;
    onCaptureFinish: () => Promise<void>;
};

export type SelectLayerProps = {
    actionRef: React.RefObject<SelectLayerActionType | undefined>;
};

const SelectLayerCore: React.FC<SelectLayerProps> = ({ actionRef }) => {
    const captureBoundingBoxInfoRef = useRef<CaptureBoundingBoxInfo | undefined>(undefined);
    const resizeToolbarActionRef = useRef<ResizeToolbarActionType | undefined>(undefined);

    const { finishCapture, drawToolbarActionRef, colorPickerActionRef, mousePositionRef } =
        useContext(DrawContext);
    const [isEnable, setIsEnable] = useState(false);

    const [findChildrenElements, setFindChildrenElements] = useState(false);
    const [selectRectRadiusCache, setSelectRectRadiusCache] = useState(0);
    const [selectRectShadowConfigCache, setSelectRectShadowConfigCache] = useState({
        shadowWidth: 0,
        shadowColor: '#00000000',
    });
    const fullScreenAuxiliaryLineColorRef = useRef<string | undefined>(undefined);
    const monitorCenterAuxiliaryLineColorRef = useRef<string | undefined>(undefined);
    const selectRectMaskColorRef = useRef<string | undefined>(undefined);
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const [getAppSettings] = useStateSubscriber(
        AppSettingsPublisher,
        useCallback((settings: AppSettingsData) => {
            setFindChildrenElements(
                settings[AppSettingsGroup.FunctionScreenshot].findChildrenElements,
            );
            setSelectRectRadiusCache(settings[AppSettingsGroup.Cache].selectRectRadius);
            setSelectRectShadowConfigCache({
                shadowWidth: settings[AppSettingsGroup.Cache].selectRectShadowWidth,
                shadowColor: settings[AppSettingsGroup.Cache].selectRectShadowColor,
            });
            const fullScreenAuxiliaryLineColor =
                settings[AppSettingsGroup.Screenshot].fullScreenAuxiliaryLineColor;
            if (new Color(fullScreenAuxiliaryLineColor).alpha() === 0) {
                fullScreenAuxiliaryLineColorRef.current = undefined;
            } else {
                fullScreenAuxiliaryLineColorRef.current = fullScreenAuxiliaryLineColor;
            }
            const monitorCenterAuxiliaryLineColor =
                settings[AppSettingsGroup.Screenshot].monitorCenterAuxiliaryLineColor;
            if (new Color(monitorCenterAuxiliaryLineColor).alpha() === 0) {
                monitorCenterAuxiliaryLineColorRef.current = undefined;
            } else {
                monitorCenterAuxiliaryLineColorRef.current = monitorCenterAuxiliaryLineColor;
            }
            const selectRectMaskColor = settings[AppSettingsGroup.Screenshot].selectRectMaskColor;
            if (selectRectMaskColor === getMaskBackgroundColor(false)) {
                selectRectMaskColorRef.current = undefined;
            } else {
                selectRectMaskColorRef.current = selectRectMaskColor;
            }
        }, []),
    );
    const [getScreenshotType] = useStateSubscriber(ScreenshotTypePublisher, undefined);
    const [tabFindChildrenElements, setTabFindChildrenElements, tabFindChildrenElementsRef] =
        useStateRef<boolean>(false); // Tab 键的切换查找子元素
    const isEnableFindChildrenElements = useCallback(() => {
        if (getPlatform() === 'macos') {
            return false;
        }

        if (!findChildrenElements) {
            return false;
        }

        return (
            tabFindChildrenElementsRef.current && getScreenshotType() !== ScreenshotType.TopWindow
        );
    }, [findChildrenElements, getScreenshotType, tabFindChildrenElementsRef]);

    const changeCursor = useCallback((cursor: string) => {
        if (layerContainerElementRef.current) {
            layerContainerElementRef.current.style.cursor = cursor;
        }
    }, []);

    const layerContainerElementRef = useRef<HTMLDivElement | null>(null);
    const selectLayerCanvasRef = useRef<HTMLCanvasElement | null>(null);
    const selectRectRadiusRef = useRef(0); // 选区圆角
    const selectRectShadowConfigRef = useRef({
        shadowWidth: 0,
        shadowColor: '#00000000',
    }); // 选区阴影宽度
    const selectLayerCanvasContextRef = useRef<CanvasRenderingContext2D | null>(null);
    const elementsListRef = useRef<ElementRect[]>([]); // 窗口元素的列表
    const elementsIndexWindowIdMapRef = useRef<Map<number, number>>(new Map()); // 窗口元素对应的窗口 ID
    const selectedWindowIdRef = useRef<number | undefined>(undefined); // 选中的窗口 ID
    const elementsListRTreeRef = useRef<Flatbush | undefined>(undefined); // 窗口元素的 RTree
    const selectWindowElementLoadingRef = useRef(true); // 是否正在加载元素选择功能
    const selectWindowFromMousePositionLevelRef = useRef(0);
    const lastMouseMovePositionRef = useRef<MousePosition | undefined>(undefined); // 上一次鼠标移动事件触发的参数
    const drawSelectRectAnimationRef = useRef<TweenAnimation<ElementRect> | undefined>(undefined); // 绘制选取框的动画
    const currentActiveMonitorRectRef = useRef<ElementRect | undefined>(undefined);
    const selectStateRef = useRef(SelectState.Auto); // 当前的选择状态
    const [getCaptureEvent] = useStateSubscriber(
        CaptureEventPublisher,
        useCallback((event: CaptureEventParams | undefined) => {
            if (event?.event === CaptureEvent.onCaptureFinish) {
                // 清除一些可能保留的状态
                mouseDownPositionRef.current = undefined;
            }
        }, []),
    );

    const tryEnableToolbar = useCallback(() => {
        if (
            selectStateRef.current !== SelectState.Selected ||
            getCaptureEvent()?.event !== CaptureEvent.onCaptureLoad
        ) {
            return;
        }

        drawToolbarActionRef.current?.setEnable(true);
    }, [drawToolbarActionRef, getCaptureEvent]);

    const setSelectState = useCallback(
        (state: SelectState) => {
            selectStateRef.current = state;
            resizeToolbarActionRef.current?.setSelectState(state);

            if (state === SelectState.Selected) {
                tryEnableToolbar();
            } else {
                drawToolbarActionRef.current?.setEnable(false);
                changeCursor('crosshair');
            }
        },
        [changeCursor, drawToolbarActionRef, tryEnableToolbar],
    );
    useStateSubscriber(CaptureEventPublisher, tryEnableToolbar);

    const mouseDownPositionRef = useRef<MousePosition | undefined>(undefined); // 鼠标按下时的位置
    const dragModeRef = useRef<DragMode | undefined>(undefined); // 拖动模式
    const dragRectRef = useRef<ElementRect | undefined>(undefined); // 拖动矩形

    const [, setDrawEvent] = useStateSubscriber(DrawEventPublisher, undefined);
    const [captureStep, setCaptureStep] = useState(CaptureStep.Select);
    const enableSelectRef = useRef(false); // 是否启用选择
    const updateEnableSelect = useCallback((captureStep: CaptureStep) => {
        enableSelectRef.current = captureStep === CaptureStep.Select;
    }, []);
    const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const [getCaptureStep] = useStateSubscriber(
        CaptureStepPublisher,
        useCallback(
            (captureStep: CaptureStep) => {
                setCaptureStep(captureStep);
                updateEnableSelect(captureStep);
            },
            [updateEnableSelect],
        ),
    );

    const getSelectRect = useCallback(() => {
        return drawSelectRectAnimationRef.current?.getTargetObject();
    }, []);

    const getSelectRectParams = useCallback(() => {
        const selectRect = getSelectRect();
        if (!selectRect) {
            return undefined;
        }

        return {
            rect: selectRect,
            radius: selectRectRadiusRef.current,
            shadowWidth: selectRectShadowConfigRef.current.shadowWidth,
            shadowColor: selectRectShadowConfigRef.current.shadowColor,
        };
    }, [getSelectRect]);

    /**
     * 非选择状态时，是否可以激活选区
     */
    const canEnableSelect = useCallback(() => {
        const selectRect = getSelectRect();
        if (!selectRect) {
            return false;
        }

        const mousePosition = mousePositionRef.current.scale(window.devicePixelRatio);

        const tolerance = EDGE_DETECTION_TOLERANCE * window.devicePixelRatio;

        // 在选区的边框的附近，但是不在选区框内
        if (
            positoinInRect(
                {
                    min_x: selectRect.min_x - tolerance,
                    min_y: selectRect.min_y - tolerance,
                    max_x: selectRect.max_x + tolerance,
                    max_y: selectRect.max_y + tolerance,
                },
                mousePosition,
            ) &&
            !positoinInRect(
                {
                    min_x: selectRect.min_x + tolerance,
                    min_y: selectRect.min_y + tolerance,
                    max_x: selectRect.max_x - tolerance,
                    max_y: selectRect.max_y - tolerance,
                },
                mousePosition,
            )
        ) {
            return true;
        }

        return false;
    }, [getSelectRect, mousePositionRef]);

    const updateLayerPointerEvents = useCallback((): boolean => {
        if (!layerContainerElementRef.current) {
            return false;
        }

        const enableSelect = enableSelectRef.current || canEnableSelect();
        layerContainerElementRef.current.style.pointerEvents = enableSelect ? 'auto' : 'none';

        return enableSelect;
    }, [canEnableSelect]);

    /**
     * 初始化元素选择功能
     */
    const initSelectWindowElement = useCallback(async () => {
        selectWindowElementLoadingRef.current = true;

        const windowElementsPromise = getWindowElements();

        const rectList: ElementRect[] = [];
        const initUiElementsCachePromise = initUiElementsCache();
        const map = new Map<number, number>();

        const windowElements = await windowElementsPromise;

        if (getPlatform() === 'macos') {
            windowElements.push({
                window_id: 0,
                element_rect: {
                    min_x: -Number.MAX_SAFE_INTEGER,
                    min_y: -Number.MAX_SAFE_INTEGER,
                    max_x: Number.MAX_SAFE_INTEGER,
                    max_y: Number.MAX_SAFE_INTEGER,
                },
            });
        }

        const rTree = new Flatbush(windowElements.length);
        windowElements.forEach((windowElement, index) => {
            const rect = windowElement.element_rect;
            rectList.push(rect);

            rTree.add(rect.min_x, rect.min_y, rect.max_x, rect.max_y);
            map.set(index, windowElement.window_id);
        });
        rTree.finish();
        elementsListRTreeRef.current = rTree;
        elementsListRef.current = rectList;
        selectedWindowIdRef.current = undefined;
        elementsIndexWindowIdMapRef.current = map;

        await initUiElementsCachePromise;
        selectWindowElementLoadingRef.current = false;
    }, []);

    /**
     * 通过鼠标坐标获取候选框
     */
    const getElementRectFromMousePosition = useCallback(
        async (mousePosition: MousePosition): Promise<ElementRect[] | undefined> => {
            if (selectWindowElementLoadingRef.current) {
                return undefined;
            }

            const elementsRTree = elementsListRTreeRef.current;
            if (!elementsRTree) {
                return undefined;
            }

            let elementRectList = undefined;
            if (isEnableFindChildrenElements()) {
                try {
                    elementRectList = await getElementFromPosition(
                        mousePosition.mouseX,
                        mousePosition.mouseY,
                    );
                } catch {
                    // 获取元素失败，忽略
                }
            }

            let result;
            if (elementRectList) {
                result = elementRectList;
            } else {
                const rectIndexs = elementsRTree.search(
                    mousePosition.mouseX,
                    mousePosition.mouseY,
                    mousePosition.mouseX,
                    mousePosition.mouseY,
                );
                // 获取的是原始数据的索引，原始数据下标越小的，窗口层级越高，所以优先选择下标小的
                rectIndexs.sort((a, b) => a - b);

                result = rectIndexs.map((index) => {
                    return elementsListRef.current[index];
                });

                selectedWindowIdRef.current = elementsIndexWindowIdMapRef.current.get(
                    rectIndexs[0],
                );
            }

            return result;
        },
        [isEnableFindChildrenElements],
    );

    const resizeToolbarUpdateStyle = useCallback(
        (rect: ElementRect) => {
            resizeToolbarActionRef.current?.updateStyle(rect);
        },
        [resizeToolbarActionRef],
    );
    const resizeToolbarUpdateStyleRenderCallback = useCallbackRenderSlow(resizeToolbarUpdateStyle);

    const { currentTheme } = useContext(AppContext);

    const {
        contentScale: [, , contentScaleRef],
    } = useMonitorRect();
    const drawCanvasSelectRect = useCallback(
        (
            rect: ElementRect,
            captureBoundingBoxInfo: CaptureBoundingBoxInfo,
            drawElementMask?: {
                imageData: ImageData;
            },
            enableScrollScreenshot?: boolean,
            enableScanQrcode?: boolean,
        ) => {
            const enableAuxiliaryLine =
                selectStateRef.current === SelectState.Auto ||
                selectStateRef.current === SelectState.Manual;

            drawSelectRect(
                captureBoundingBoxInfo.width,
                captureBoundingBoxInfo.height,
                rect,
                selectRectRadiusRef.current,
                selectLayerCanvasContextRef.current!,
                currentTheme === AppSettingsTheme.Dark,
                contentScaleRef.current,
                getScreenshotType() === ScreenshotType.TopWindow ||
                    selectStateRef.current === SelectState.Auto,
                drawElementMask,
                enableScrollScreenshot,
                enableScanQrcode,
                enableAuxiliaryLine &&
                    lastMouseMovePositionRef.current &&
                    fullScreenAuxiliaryLineColorRef.current
                    ? {
                          mousePosition: lastMouseMovePositionRef.current,
                          color: fullScreenAuxiliaryLineColorRef.current,
                      }
                    : undefined,
                enableAuxiliaryLine &&
                    monitorCenterAuxiliaryLineColorRef.current &&
                    currentActiveMonitorRectRef.current
                    ? {
                          activeMonitorRect: currentActiveMonitorRectRef.current,
                          color: monitorCenterAuxiliaryLineColorRef.current,
                      }
                    : undefined,
                selectRectMaskColorRef.current,
            );

            // 和 canvas 同步下
            resizeToolbarUpdateStyleRenderCallback(rect);
        },
        [contentScaleRef, currentTheme, getScreenshotType, resizeToolbarUpdateStyleRenderCallback],
    );

    const publishSelectRectAnimationParams = useCallback(
        (rect: ElementRect) => {
            setDrawEvent({
                event: DrawEvent.SelectRectParamsAnimationChange,
                params: {
                    selectRectParams: {
                        rect,
                        radius: selectRectRadiusRef.current,
                        shadowWidth: selectRectShadowConfigRef.current.shadowWidth,
                        shadowColor: selectRectShadowConfigRef.current.shadowColor,
                    },
                },
            });
            setDrawEvent(undefined);
        },
        [setDrawEvent],
    );

    const initAnimation = useCallback(
        (captureBoundingBoxInfo: CaptureBoundingBoxInfo) => {
            if (drawSelectRectAnimationRef.current) {
                drawSelectRectAnimationRef.current.dispose();
            }

            drawSelectRectAnimationRef.current = new TweenAnimation<ElementRect>(
                {
                    min_x: 0,
                    min_y: 0,
                    max_x: captureBoundingBoxInfo.rect.max_x,
                    max_y: captureBoundingBoxInfo.rect.max_y,
                },
                TWEEN.Easing.Quadratic.Out,
                100,
                (rect) => {
                    drawCanvasSelectRect(rect, captureBoundingBoxInfo);
                    publishSelectRectAnimationParams(rect);
                },
            );
        },
        [drawCanvasSelectRect, publishSelectRectAnimationParams],
    );

    const onCaptureBoundingBoxInfoReady = useCallback<
        SelectLayerActionType['onCaptureBoundingBoxInfoReady']
    >(
        async (captureBoundingBoxInfo): Promise<void> => {
            captureBoundingBoxInfoRef.current = captureBoundingBoxInfo;

            // 初始化下坐标，用来在触发鼠标移动事件前选取坐标
            lastMouseMovePositionRef.current = captureBoundingBoxInfo.mousePosition;
            // 初始化下选择状态
            setSelectState(SelectState.Auto);
            // 清除 mouseDownPosition，避免拖动时提前退出，第二次唤醒时依旧保留了状态
            mouseDownPositionRef.current = undefined;

            if (!selectLayerCanvasContextRef.current) {
                selectLayerCanvasContextRef.current =
                    selectLayerCanvasRef.current!.getContext('2d');
            }

            selectLayerCanvasRef.current!.height = captureBoundingBoxInfo.height;
            selectLayerCanvasRef.current!.width = captureBoundingBoxInfo.width;

            initAnimation(captureBoundingBoxInfo);
        },
        [initAnimation, setSelectState],
    );

    const opacityImageDataRef = useRef<
        | {
              opacity: number;
              imageData: ImageData;
          }
        | undefined
    >(undefined);
    const renderElementMask = useCallback(
        async (isEnable: boolean) => {
            if (isEnable) {
                // 如果有缓存，则把遮罩去除
                if (opacityImageDataRef.current && captureBoundingBoxInfoRef.current) {
                    drawCanvasSelectRect(getSelectRect()!, captureBoundingBoxInfoRef.current);
                }

                return;
            }

            const opacity = Math.min(
                Math.max(
                    (100 -
                        getAppSettings()[AppSettingsGroup.Screenshot]
                            .beyondSelectRectElementOpacity) /
                        100,
                    0,
                ),
                1,
            );

            let imageData: ImageData | undefined;
            if (opacity === 0) {
                imageData = undefined;
            } else if (
                opacityImageDataRef.current &&
                opacityImageDataRef.current.opacity === opacity
            ) {
                imageData = opacityImageDataRef.current.imageData;
            } else {
                const originalImageData = await colorPickerActionRef.current?.getPreviewImageData();

                if (originalImageData) {
                    let newImageData: ImageData;
                    if (opacity === 1) {
                        newImageData = originalImageData;
                    } else {
                        newImageData = new ImageData(
                            originalImageData.data,
                            originalImageData.width,
                            originalImageData.height,
                        );

                        for (let i = 3; i < newImageData.data.length; i += 4) {
                            newImageData.data[i] = Math.round(newImageData.data[i] * opacity);
                        }
                    }

                    imageData = newImageData;
                    opacityImageDataRef.current = {
                        opacity,
                        imageData: newImageData,
                    };
                }
            }

            if (!imageData || !captureBoundingBoxInfoRef.current) {
                return;
            }

            drawCanvasSelectRect(
                getSelectRect()!,
                captureBoundingBoxInfoRef.current,
                imageData
                    ? {
                          imageData,
                      }
                    : undefined,
            );
        },
        [colorPickerActionRef, getAppSettings, getSelectRect, drawCanvasSelectRect],
    );

    const onCaptureFinish = useCallback<BaseLayerEventActionType['onCaptureFinish']>(async () => {
        selectLayerCanvasContextRef.current?.clearRect(
            0,
            0,
            selectLayerCanvasContextRef.current.canvas.width,
            selectLayerCanvasContextRef.current.canvas.height,
        );
        captureBoundingBoxInfoRef.current = undefined;
        selectWindowElementLoadingRef.current = true;
        elementsListRTreeRef.current = undefined;
        elementsListRef.current = [];
        lastMouseMovePositionRef.current = undefined;
        opacityImageDataRef.current = undefined;
    }, []);

    const autoSelect = useCallback(
        async (mousePosition: MousePosition): Promise<ElementRect> => {
            const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;
            if (!captureBoundingBoxInfo) {
                return {
                    min_x: 0,
                    min_y: 0,
                    max_x: document.body.clientWidth * window.devicePixelRatio,
                    max_y: document.body.clientHeight * window.devicePixelRatio,
                };
            }

            let elementRectList = await getElementRectFromMousePosition(mousePosition);

            if (!elementRectList || elementRectList.length === 0) {
                elementRectList =
                    getScreenshotType() === ScreenshotType.TopWindow
                        ? [{ min_x: 0, min_y: 0, max_x: 0, max_y: 0 }]
                        : [
                              captureBoundingBoxInfo.getActiveMonitorRect({
                                  min_x: mousePosition.mouseX,
                                  min_y: mousePosition.mouseY,
                                  max_x: mousePosition.mouseX,
                                  max_y: mousePosition.mouseY,
                              }),
                          ];
            }

            const minLevel = 0;
            const maxLevel = Math.max(elementRectList.length - 1, minLevel);
            let currentLevel = selectWindowFromMousePositionLevelRef.current;
            if (currentLevel < minLevel) {
                currentLevel = minLevel;
            } else if (currentLevel > maxLevel) {
                currentLevel = maxLevel;
                selectWindowFromMousePositionLevelRef.current = maxLevel;
            }

            let selectedRect = {
                min_x: elementRectList[currentLevel].min_x,
                min_y: elementRectList[currentLevel].min_y,
                max_x: elementRectList[currentLevel].max_x,
                max_y: elementRectList[currentLevel].max_y,
            };

            selectedRect = limitRect(
                selectedRect,
                currentLevel === elementRectList.length - 1
                    ? captureBoundingBoxInfo.getActiveMonitorRect({
                          min_x: mousePosition.mouseX,
                          min_y: mousePosition.mouseY,
                          max_x: mousePosition.mouseX,
                          max_y: mousePosition.mouseY,
                      })
                    : captureBoundingBoxInfo.rect,
            );

            return captureBoundingBoxInfo.transformMonitorRect(selectedRect);
        },
        [getElementRectFromMousePosition, getScreenshotType],
    );

    const updateDragMode = useCallback(
        (mousePosition: MousePosition): DragMode => {
            dragModeRef.current = getDragModeFromMousePosition(getSelectRect()!, mousePosition);

            changeCursor(convertDragModeToCursor(dragModeRef.current));

            return dragModeRef.current;
        },
        [changeCursor, getSelectRect],
    );
    const updateDragModeRenderCallback = useCallbackRender(updateDragMode);

    const updateMonitorRect = useCallback(
        (rect: ElementRect) => {
            const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;

            if (!captureBoundingBoxInfo) {
                return;
            }

            const activeMonitor = captureBoundingBoxInfo.getActiveMonitor(
                captureBoundingBoxInfo.transformWindowRect(rect),
                true,
            );
            currentActiveMonitorRectRef.current = captureBoundingBoxInfo.transformMonitorRect(
                activeMonitor.rect,
            );

            setDrawEvent({
                event: DrawEvent.ChangeMonitor,
                params: {
                    rect: {
                        rect: currentActiveMonitorRectRef.current,
                        scale_factor: activeMonitor.scale_factor,
                    },
                },
            });
            setDrawEvent(undefined);
        },
        [setDrawEvent],
    );
    const updateMonitorRectRenderCallback = useCallbackRenderSlow(updateMonitorRect);

    const setSelectRect = useCallback(
        (rect: ElementRect, ignoreAnimation: boolean = false, forceUpdate: boolean = false) => {
            if (forceUpdate) {
                if (captureBoundingBoxInfoRef.current) {
                    drawCanvasSelectRect(rect, captureBoundingBoxInfoRef.current);
                }
            } else {
                drawSelectRectAnimationRef.current?.update(
                    rect,
                    ignoreAnimation ||
                        getAppSettings()[AppSettingsGroup.Screenshot].disableAnimation,
                );
            }
            resizeToolbarActionRef.current?.setSelectedRect(rect);

            updateMonitorRectRenderCallback(rect);
        },
        [updateMonitorRectRenderCallback, drawCanvasSelectRect, getAppSettings],
    );

    const previousDrawStateRef = useRef<DrawState | undefined>(undefined);
    const onMouseDown = useCallback(
        (mousePosition: MousePosition) => {
            if (!enableSelectRef.current) {
                // 响应了鼠标事件，但是未启用选择，说明是可激活状态，将工具栏切换为 Idle
                if (getCaptureStep() !== CaptureStep.Draw) {
                    return;
                }

                previousDrawStateRef.current = getDrawState();
                drawToolbarActionRef.current?.onToolClick(DrawState.Idle);
            }

            mouseDownPositionRef.current = mousePosition;
            if (selectStateRef.current === SelectState.Auto) {
            } else if (selectStateRef.current === SelectState.Selected) {
                // 改变状态为拖动
                setSelectState(SelectState.Drag);
                updateDragMode(mousePosition);
                dragRectRef.current = getSelectRect()!;
            }
        },
        [
            drawToolbarActionRef,
            getCaptureStep,
            getDrawState,
            getSelectRect,
            setSelectState,
            updateDragMode,
        ],
    );

    const onMouseMoveAutoSelectCore = useCallback(
        async (mousePosition: MousePosition, ignoreAnimation: boolean = false) => {
            const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;
            if (!captureBoundingBoxInfo) {
                return;
            }

            // 防止自动框选阻塞手动选择
            const currentSelectRect = await autoSelect(
                new MousePosition(
                    mousePosition.mouseX + captureBoundingBoxInfo.rect.min_x,
                    mousePosition.mouseY + captureBoundingBoxInfo.rect.min_y,
                ),
            );

            // 判断当前是否还是自动选择状态
            if (selectStateRef.current !== SelectState.Auto) {
                return;
            }

            // 注意做个纠正，防止超出显示器范围
            currentSelectRect.min_x = Math.max(currentSelectRect.min_x, 0);
            currentSelectRect.min_y = Math.max(currentSelectRect.min_y, 0);
            currentSelectRect.max_x = Math.min(
                currentSelectRect.max_x,
                captureBoundingBoxInfoRef.current?.width ?? 0,
            );
            currentSelectRect.max_y = Math.min(
                currentSelectRect.max_y,
                captureBoundingBoxInfoRef.current?.height ?? 0,
            );

            if (
                drawSelectRectAnimationRef.current?.isDone() &&
                currentSelectRect.min_x === getSelectRect()?.min_x &&
                currentSelectRect.min_y === getSelectRect()?.min_y &&
                currentSelectRect.max_x === getSelectRect()?.max_x &&
                currentSelectRect.max_y === getSelectRect()?.max_y
            ) {
                setSelectRect(currentSelectRect, true, true);
            } else {
                setSelectRect(
                    currentSelectRect,
                    ignoreAnimation || getScreenshotType() === ScreenshotType.TopWindow,
                );
            }
        },
        [autoSelect, getSelectRect, setSelectRect, getScreenshotType],
    );
    const onMouseMoveAutoSelectLastParamsRef = useRef<
        | {
              mousePosition: MousePosition;
              ignoreAnimation: boolean;
          }
        | undefined
    >(undefined);
    const onMouseMoveAutoSelectRunningRef = useRef<boolean>(false);
    const onMouseMoveAutoSelect = useCallback(
        async (mousePosition: MousePosition, ignoreAnimation: boolean = false) => {
            // 保存最新的参数
            onMouseMoveAutoSelectLastParamsRef.current = {
                mousePosition,
                ignoreAnimation,
            };

            // 如果已经在运行，直接返回，等待当前执行完成
            if (onMouseMoveAutoSelectRunningRef.current) {
                return;
            }

            // 标记开始运行
            onMouseMoveAutoSelectRunningRef.current = true;

            try {
                // 循环处理，直到没有新的参数
                while (onMouseMoveAutoSelectLastParamsRef.current) {
                    const currentParams = onMouseMoveAutoSelectLastParamsRef.current;
                    // 清空参数，防止重复处理
                    onMouseMoveAutoSelectLastParamsRef.current = undefined;

                    // 执行核心逻辑
                    await onMouseMoveAutoSelectCore(
                        currentParams.mousePosition,
                        currentParams.ignoreAnimation,
                    );
                }
            } finally {
                // 确保标记为未运行
                onMouseMoveAutoSelectRunningRef.current = false;
            }
        },
        [onMouseMoveAutoSelectCore],
    );

    const { disableMouseMove, enableMouseMove, isDisableMouseMove } = useMoveCursor();
    const onMouseMove = useCallback(
        (mousePosition: MousePosition, ignoreAnimation: boolean = false) => {
            if (!enableSelectRef.current) {
                return;
            }

            // 恢复鼠标事件
            enableMouseMove();

            // 检测下鼠标移动的距离
            lastMouseMovePositionRef.current = mousePosition;

            if (selectStateRef.current === SelectState.Auto) {
                if (
                    mouseDownPositionRef.current &&
                    getScreenshotType() !== ScreenshotType.TopWindow
                ) {
                    // 检测拖动距离是否启用手动选择
                    const maxSide = mouseDownPositionRef.current.getMaxSide(mousePosition);
                    if (maxSide > 6) {
                        setSelectState(SelectState.Manual);
                    }
                }

                // 防止自动框选阻塞手动选择，使用异步方案
                onMouseMoveAutoSelect(mousePosition, ignoreAnimation);
            } else if (selectStateRef.current === SelectState.Manual) {
                if (!mouseDownPositionRef.current) {
                    return;
                }

                setSelectRect(
                    limitRect(
                        mouseDownPositionRef.current.toElementRect(
                            mousePosition,
                            isHotkeyPressed(
                                getAppSettings()[AppSettingsGroup.DrawToolbarKeyEvent][
                                    KeyEventKey.LockWidthHeightPicker
                                ].hotKey,
                            ),
                        ),
                        {
                            min_x: 0,
                            min_y: 0,
                            max_x: captureBoundingBoxInfoRef.current!.width,
                            max_y: captureBoundingBoxInfoRef.current!.height,
                        },
                        true,
                    ),
                    true,
                );
            } else if (selectStateRef.current === SelectState.Selected) {
                updateDragMode(mousePosition);
            } else if (selectStateRef.current === SelectState.Drag) {
                if (!mouseDownPositionRef.current) {
                    return;
                }

                setSelectRect(
                    dragRect(
                        dragModeRef.current!,
                        dragRectRef.current!,
                        mouseDownPositionRef.current,
                        mousePosition,
                        isHotkeyPressed(
                            getAppSettings()[AppSettingsGroup.DrawToolbarKeyEvent][
                                KeyEventKey.LockWidthHeightPicker
                            ].hotKey,
                        ),
                    ),
                    true,
                );
            }
        },
        [
            enableMouseMove,
            getScreenshotType,
            onMouseMoveAutoSelect,
            setSelectState,
            getAppSettings,
            setSelectRect,
            updateDragMode,
        ],
    );
    const onMouseUp = useCallback(() => {
        if (!enableSelectRef.current) {
            return;
        }

        if (!mouseDownPositionRef.current) {
            return;
        }

        if (selectStateRef.current === SelectState.Auto) {
            setSelectState(SelectState.Selected);
            setSelectRect(getSelectRect()!, true, true);
        } else if (selectStateRef.current === SelectState.Manual) {
            setSelectState(SelectState.Selected);
            setSelectRect(getSelectRect()!, true, true);
        } else if (selectStateRef.current === SelectState.Drag) {
            setSelectState(SelectState.Selected);
            setSelectRect(
                limitRect(getSelectRect()!, {
                    min_x: 0,
                    min_y: 0,
                    max_x: captureBoundingBoxInfoRef.current!.width,
                    max_y: captureBoundingBoxInfoRef.current!.height,
                }),
            );
            dragRectRef.current = undefined;

            // 恢复之前的工具栏状态
            if (previousDrawStateRef.current) {
                drawToolbarActionRef.current?.onToolClick(previousDrawStateRef.current);
                previousDrawStateRef.current = undefined;
            }
        }

        mouseDownPositionRef.current = undefined;
    }, [drawToolbarActionRef, getSelectRect, setSelectRect, setSelectState]);

    const onMouseMoveRenderCallback = useCallbackRender(onMouseMove);
    // 用上一次的鼠标移动事件触发 onMouseMove 来更新一些状态
    const refreshMouseMove = useCallback(
        (ignoreAnimation: boolean = false) => {
            if (!lastMouseMovePositionRef.current) {
                return;
            }

            onMouseMove(lastMouseMovePositionRef.current, ignoreAnimation);
        },
        [onMouseMove],
    );
    const onMouseWheel = useCallback(
        (e: WheelEvent) => {
            if (selectStateRef.current !== SelectState.Auto) {
                return;
            }

            const deltaLevel = e.deltaY > 0 ? 1 : -1;
            selectWindowFromMousePositionLevelRef.current = Math.max(
                selectWindowFromMousePositionLevelRef.current + deltaLevel,
                0,
            );
            refreshMouseMove();
        },
        [refreshMouseMove],
    );
    const onMouseWheelRenderCallback = useCallbackRender(onMouseWheel);

    const onExecuteScreenshot = useCallback<
        BaseLayerEventActionType['onExecuteScreenshot']
    >(async () => {
        await initSelectWindowElement();

        // 初始化可能晚于截图准备
        refreshMouseMove(true);
    }, [initSelectWindowElement, refreshMouseMove]);

    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState, prevDrawState: DrawState) => {
                if (!captureBoundingBoxInfoRef.current) {
                    return;
                }

                if (
                    drawState === DrawState.ScrollScreenshot ||
                    drawState === DrawState.ScanQrcode
                ) {
                    drawCanvasSelectRect(
                        getSelectRect()!,
                        captureBoundingBoxInfoRef.current,
                        undefined,
                        drawState === DrawState.ScrollScreenshot,
                        drawState === DrawState.ScanQrcode,
                    );
                } else if (prevDrawState === DrawState.ScrollScreenshot) {
                    drawCanvasSelectRect(
                        getSelectRect()!,
                        captureBoundingBoxInfoRef.current,
                        undefined,
                        false,
                    );
                }
            },
            [getSelectRect, drawCanvasSelectRect],
        ),
    );

    useEffect(() => {
        return () => {
            drawSelectRectAnimationRef.current?.dispose();
        };
    }, []);

    useEffect(() => {
        setTabFindChildrenElements(findChildrenElements);
    }, [findChildrenElements, setTabFindChildrenElements]);
    // 查找子元素切换时，刷新选取
    useEffect(() => {
        refreshMouseMove();
    }, [tabFindChildrenElements, refreshMouseMove]);

    useEffect(() => {
        const layerContainerElement = layerContainerElementRef.current;
        if (!layerContainerElement) {
            return;
        }

        const handleMouseDown = (e: MouseEvent) => {
            if (e.button !== 0) {
                return;
            }

            onMouseDown(new MousePosition(e.clientX, e.clientY).scale(window.devicePixelRatio));
        };
        const handleMouseMove = (e: MouseEvent) => {
            if (isDisableMouseMove()) {
                return;
            }

            onMouseMoveRenderCallback(
                new MousePosition(e.clientX, e.clientY).scale(window.devicePixelRatio),
            );
        };
        const handleMouseUp = (e: MouseEvent) => {
            if (e.button !== 0) {
                return;
            }

            onMouseUp();
        };
        layerContainerElement.addEventListener('mousedown', handleMouseDown);
        layerContainerElement.addEventListener('mousemove', handleMouseMove);
        layerContainerElement.addEventListener('mouseup', handleMouseUp);
        layerContainerElement.addEventListener('wheel', onMouseWheelRenderCallback, {
            passive: true,
        });
        return () => {
            layerContainerElement.removeEventListener('mousedown', handleMouseDown);
            layerContainerElement.removeEventListener('mousemove', handleMouseMove);
            layerContainerElement.removeEventListener('mouseup', handleMouseUp);
            layerContainerElement.removeEventListener('wheel', onMouseWheelRenderCallback);
        };
    }, [
        isDisableMouseMove,
        layerContainerElementRef,
        onMouseDown,
        onMouseMoveRenderCallback,
        onMouseUp,
        onMouseWheelRenderCallback,
    ]);
    useStateSubscriber(
        DrawEventPublisher,
        useCallback(
            (drawEvent: DrawEventParams | undefined) => {
                if (drawEvent?.event === DrawEvent.MoveCursor) {
                    disableMouseMove();

                    onMouseMoveRenderCallback(
                        new MousePosition(drawEvent.params.x, drawEvent.params.y),
                    );
                }
            },
            [disableMouseMove, onMouseMoveRenderCallback],
        ),
    );

    // 选择状态未激活时，鼠标在选区边框附件依旧可以更改选区
    useEffect(() => {
        if (captureStep !== CaptureStep.Draw) {
            return;
        }

        const handleMouseMove = () => {
            if (!updateLayerPointerEvents()) {
                return;
            }

            updateDragModeRenderCallback(mousePositionRef.current.scale(window.devicePixelRatio));
        };

        document.addEventListener('mousemove', handleMouseMove);

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
        };
    }, [
        canEnableSelect,
        captureStep,
        mousePositionRef,
        selectStateRef,
        updateDragMode,
        updateDragModeRenderCallback,
        updateLayerPointerEvents,
    ]);

    const setPrevSelectRect = useCallback(
        (prevSelectRect: ElementRect) => {
            const actualPrevSelectRect = limitRect(prevSelectRect, {
                min_x: 0,
                min_y: 0,
                max_x: captureBoundingBoxInfoRef.current!.width,
                max_y: captureBoundingBoxInfoRef.current!.height,
            });

            if (
                actualPrevSelectRect.min_x < actualPrevSelectRect.max_x &&
                actualPrevSelectRect.min_y < actualPrevSelectRect.max_y
            ) {
                setSelectState(SelectState.Drag);
                setSelectRect(actualPrevSelectRect);
                setSelectState(SelectState.Selected);
            }
        },
        [setSelectRect, setSelectState],
    );

    useImperativeHandle(
        actionRef,
        () => ({
            onExecuteScreenshot,
            onCaptureBoundingBoxInfoReady: async (captureBoundingBoxInfo) => {
                await onCaptureBoundingBoxInfoReady(captureBoundingBoxInfo);
                refreshMouseMove(true);
            },
            getWindowId: () => selectedWindowIdRef.current,
            onCaptureFinish,
            getSelectRect,
            getSelectRectParams,
            setEnable: (enable: boolean) => {
                setIsEnable(enable);
            },
            getSelectState: () => selectStateRef.current,
            switchCaptureHistory: (captureHistory: CaptureHistoryItem | undefined) => {
                // 清除遮罩缓存
                opacityImageDataRef.current = undefined;

                if (!captureHistory) {
                    setSelectState(SelectState.Auto);
                    refreshMouseMove();
                    return;
                }

                setPrevSelectRect(captureHistory.selected_rect);
            },
        }),
        [
            getSelectRect,
            getSelectRectParams,
            onCaptureBoundingBoxInfoReady,
            onCaptureFinish,
            onExecuteScreenshot,
            refreshMouseMove,
            setPrevSelectRect,
            setSelectState,
        ],
    );

    useEffect(() => {
        if (!isEnable) {
            return;
        }

        const onKeyDown = (e: KeyboardEvent) => {
            if (!enableSelectRef.current) {
                return;
            }

            if (isHotkeyPressed('Tab')) {
                setTabFindChildrenElements((prev) => !prev);
                e.preventDefault();
                return;
            }

            if (
                isHotkeyPressed(
                    getAppSettings()[AppSettingsGroup.DrawToolbarKeyEvent][KeyEventKey.CancelTool]
                        .hotKey,
                ) &&
                selectStateRef.current !== SelectState.Selected
            ) {
                finishCapture();

                e.preventDefault();
                return;
            }

            if (
                isHotkeyPressed(
                    getAppSettings()[AppSettingsGroup.DrawToolbarKeyEvent][
                        KeyEventKey.SelectPrevRectTool
                    ].hotKey,
                )
            ) {
                const prevSelectRect = getAppSettings()[AppSettingsGroup.Cache].prevSelectRect;
                if (
                    prevSelectRect.min_x < prevSelectRect.max_x &&
                    prevSelectRect.min_y < prevSelectRect.max_y
                ) {
                    setPrevSelectRect(prevSelectRect);
                }

                e.preventDefault();
                return;
            }
        };

        document.addEventListener('keydown', onKeyDown);

        return () => {
            document.removeEventListener('keydown', onKeyDown);
        };
    }, [
        finishCapture,
        getAppSettings,
        getSelectRect,
        isEnable,
        setPrevSelectRect,
        setSelectRect,
        setSelectState,
        setTabFindChildrenElements,
    ]);

    useEffect(() => {
        updateLayerPointerEvents();

        // 切换为其他功能时，渲染元素遮罩
        renderElementMask(isEnable);

        if (!isEnable) {
            return;
        }

        const mouseRightClick = (e: MouseEvent) => {
            if (process.env.NODE_ENV === 'development' && getPlatform() === 'macos') {
                return;
            }

            e.preventDefault();
            // 回退到选择
            if (selectStateRef.current === SelectState.Selected) {
                setSelectState(SelectState.Auto);
                refreshMouseMove();
            } else if (selectStateRef.current === SelectState.Auto) {
                // 取消截图
                finishCapture();
            }
        };

        document.addEventListener('contextmenu', mouseRightClick);

        return () => {
            document.removeEventListener('contextmenu', mouseRightClick);
        };
    }, [
        finishCapture,
        isEnable,
        refreshMouseMove,
        renderElementMask,
        setSelectState,
        updateLayerPointerEvents,
    ]);

    const onSelectedRectChangeCompleted = useMemo(() => {
        return debounce(() => {
            setSelectState(SelectState.Selected);
        }, 256);
    }, [setSelectState]);
    const onSelectedRectChange = useCallback(
        (rect: ElementRect) => {
            setSelectState(SelectState.ScrollResize);
            setSelectRect(
                limitRect(rect, {
                    min_x: 0,
                    min_y: 0,
                    max_x: captureBoundingBoxInfoRef.current!.width,
                    max_y: captureBoundingBoxInfoRef.current!.height,
                }),
                true,
            );
            onSelectedRectChangeCompleted();
        },
        [setSelectRect, setSelectState, onSelectedRectChangeCompleted],
    );
    const onRadiusChange = useCallback(
        (radius: number) => {
            selectRectRadiusRef.current = radius;
            resizeToolbarActionRef.current?.setRadius(radius);

            const selectRect = getSelectRect();
            if (selectRect) {
                drawCanvasSelectRect(
                    selectRect,
                    captureBoundingBoxInfoRef.current!,
                    undefined,
                    false,
                );
                updateAppSettings(
                    AppSettingsGroup.Cache,
                    { selectRectRadius: radius },
                    true,
                    true,
                    false,
                    true,
                    true,
                );
                publishSelectRectAnimationParams(selectRect);
            }
        },
        [drawCanvasSelectRect, getSelectRect, publishSelectRectAnimationParams, updateAppSettings],
    );
    useEffect(() => {
        if (selectRectRadiusRef.current !== selectRectRadiusCache) {
            onRadiusChange(selectRectRadiusCache);
        }
    }, [selectRectRadiusCache, onRadiusChange]);

    const onShadowConfigChange = useCallback(
        (shadowConfig: { shadowWidth: number; shadowColor: string }) => {
            selectRectShadowConfigRef.current = shadowConfig;
            resizeToolbarActionRef.current?.setShadowConfig(shadowConfig);

            const selectRect = getSelectRect();
            if (selectRect) {
                drawCanvasSelectRect(
                    selectRect,
                    captureBoundingBoxInfoRef.current!,
                    undefined,
                    false,
                );
                updateAppSettings(
                    AppSettingsGroup.Cache,
                    {
                        selectRectShadowWidth: shadowConfig.shadowWidth,
                        selectRectShadowColor: shadowConfig.shadowColor,
                    },
                    true,
                    true,
                    false,
                    true,
                    true,
                );
                publishSelectRectAnimationParams(selectRect);
            }
        },
        [drawCanvasSelectRect, getSelectRect, publishSelectRectAnimationParams, updateAppSettings],
    );
    useEffect(() => {
        if (
            selectRectShadowConfigRef.current.shadowWidth !==
                selectRectShadowConfigCache.shadowWidth ||
            selectRectShadowConfigRef.current.shadowColor !==
                selectRectShadowConfigCache.shadowColor
        ) {
            onShadowConfigChange(selectRectShadowConfigCache);
        }
    }, [selectRectShadowConfigCache, onShadowConfigChange]);

    const getCaptureBoundingBoxInfo = useCallback(
        () => captureBoundingBoxInfoRef.current,
        [captureBoundingBoxInfoRef],
    );
    return (
        <>
            <ResizeToolbar
                actionRef={resizeToolbarActionRef}
                onSelectedRectChange={onSelectedRectChange}
                onRadiusChange={onRadiusChange}
                onShadowConfigChange={onShadowConfigChange}
                getCaptureBoundingBoxInfo={getCaptureBoundingBoxInfo}
            />

            <div className="select-layer-container" ref={layerContainerElementRef}>
                <canvas className="select-layer-canvas" ref={selectLayerCanvasRef} />
                <style jsx>{`
                    .select-layer-container {
                        position: fixed;
                        top: 0;
                        left: 0;
                        width: 100vw;
                        height: 100vh;
                        z-index: ${zIndexs.Draw_SelectLayer};
                    }

                    .select-layer-container > .select-layer-canvas {
                        width: 100vw;
                        height: 100vh;
                        position: absolute;
                        top: 0;
                        left: 0;
                    }
                `}</style>
            </div>
        </>
    );
};

export default React.memo(SelectLayerCore);
