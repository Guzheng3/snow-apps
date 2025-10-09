'use client';

import { zIndexs } from '@/utils/zIndex';
import { CaptureStep, DrawContext } from '../../types';
import { useCallback, useContext, useImperativeHandle, useMemo, useRef, useState } from 'react';
import { Flex, theme } from 'antd';
import React from 'react';
import { DragButton, DragButtonActionType } from './components/dragButton';
import { DrawToolbarContext } from './extra';
import { KeyEventKey } from './components/keyEventWrap/extra';
import { CloseOutlined, CopyOutlined, DragOutlined, LockOutlined } from '@ant-design/icons';
import {
    ArrowIcon,
    ArrowSelectIcon,
    CircleIcon,
    EraserIcon,
    FastSaveIcon,
    FilterIcon,
    FixedIcon,
    OcrDetectIcon,
    OcrTranslateIcon,
    PenIcon,
    RectIcon,
    SaveIcon,
    ScrollScreenshotIcon,
    SerialNumberIcon,
    TextIcon,
} from '@/components/icons';
import {
    CaptureEvent,
    CaptureEventParams,
    CaptureEventPublisher,
    CaptureStepPublisher,
    DrawEvent,
    DrawEventParams,
    DrawEventPublisher,
    ScreenshotTypePublisher,
} from '../../extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { createPublisher } from '@/hooks/useStatePublisher';
import { EnableKeyEventPublisher } from './components/keyEventWrap/extra';
import { HistoryControls } from './components/historyControls';
import { ToolButton } from './components/toolButton';
import { FormattedMessage, useIntl } from 'react-intl';
import { BlurTool } from './components/tools/blurTool';
import { ScreenshotType } from '@/functions/screenshot';
import {
    ScrollScreenshot,
    ScrollScreenshotActionType,
} from './components/tools/scrollScreenshotTool';
import { AntdContext } from '@/components/globalLayoutExtra';
import {
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
    CanHiddenToolSet,
} from '@/app/contextWrap';
import { debounce } from 'es-toolkit';
import {
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventParams,
    ExcalidrawEventPublisher,
} from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateRef } from '@/hooks/useStateRef';
import { getExcalidrawCanvas } from '@/utils/excalidraw';
import { ExtraTool } from './components/tools/extraTool';
import { DrawExtraTool } from './components/tools/drawExtraTool';
import { HighlightTool } from './components/tools/highlightTool';
import { PLUGIN_ID_RAPID_OCR, usePluginService } from '@/components/pluginService';

export type DrawToolbarProps = {
    actionRef: React.RefObject<DrawToolbarActionType | undefined>;
    onCancel: () => void;
    onSave: (fastSave?: boolean) => void;
    onFixed: () => void;
    onTopWindow: () => void;
    onCopyToClipboard: () => void;
    onOcrDetect: () => void;
};

export type DrawToolbarActionType = {
    setEnable: (enable: boolean) => void;
    onToolClick: (drawState: DrawState) => void;
};

export const DrawToolbarStatePublisher = createPublisher<{
    mouseHover: boolean;
}>({
    mouseHover: false,
});

const isDrawTool = (drawState: DrawState) => {
    switch (drawState) {
        case DrawState.Rect:
        case DrawState.Diamond:
        case DrawState.Ellipse:
        case DrawState.Arrow:
        case DrawState.Line:
        case DrawState.Pen:
        case DrawState.Text:
        case DrawState.SerialNumber:
        case DrawState.Blur:
        case DrawState.BlurFreeDraw:
        case DrawState.Watermark:
        case DrawState.Highlight:
            return true;
        default:
            return false;
    }
};

const DrawToolbarCore: React.FC<DrawToolbarProps> = ({
    actionRef,
    onCancel,
    onSave,
    onFixed,
    onCopyToClipboard,
    onTopWindow,
    onOcrDetect,
}) => {
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const { drawCacheLayerActionRef, selectLayerActionRef } = useContext(DrawContext);

    const { token } = theme.useToken();
    const { message } = useContext(AntdContext);
    const intl = useIntl();

    const enableRef = useRef(false);
    const [showLockDrawTool, setShowLockDrawTool, showLockDrawToolRef] = useStateRef(false);
    const [enableLockDrawTool, setEnableLockDrawTool, enableLockDrawToolRef] = useStateRef(false);
    const [enableFastSave, setEnableFastSave] = useState(false);
    const [enableScrollScreenshot, setEnableScrollScreenshot] = useState(false);
    const [shortcutCanleTip, setShortcutCanleTip] = useState(false);
    const [customToolbarToolHiddenMap, setCustomToolbarToolHiddenMap] = useState<
        Partial<Record<DrawState, boolean>> | undefined
    >(undefined);
    const drawToolarContainerRef = useRef<HTMLDivElement | null>(null);
    const drawToolbarOpacityWrapRef = useRef<HTMLDivElement | null>(null);
    const scrollScreenshotToolActionRef = useRef<ScrollScreenshotActionType | undefined>(undefined);
    const drawToolbarRef = useRef<HTMLDivElement | null>(null);
    const dragButtonActionRef = useRef<DragButtonActionType | undefined>(undefined);
    const [, setEnableKeyEvent] = useStateSubscriber(EnableKeyEventPublisher, undefined);

    const [getDrawToolbarState, setDrawToolbarState] = useStateSubscriber(
        DrawToolbarStatePublisher,
        undefined,
    );
    const [getDrawState, setDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const [, setCaptureStep] = useStateSubscriber(CaptureStepPublisher, undefined);
    const [getScreenshotType] = useStateSubscriber(ScreenshotTypePublisher, undefined);

    useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (settings: AppSettingsData) => {
                setShortcutCanleTip(settings[AppSettingsGroup.FunctionScreenshot].shortcutCanleTip);
                setEnableFastSave(settings[AppSettingsGroup.FunctionScreenshot].fastSave);
                // 不显示锁定绘制工具
                setShowLockDrawTool(!settings[AppSettingsGroup.FunctionDraw].lockDrawTool);
                // 是否启用锁定绘制工具
                setEnableLockDrawTool(settings[AppSettingsGroup.Cache].enableLockDrawTool);

                const toolHiddenMap: Partial<Record<DrawState, boolean>> = {};
                for (const drawState of CanHiddenToolSet.values()) {
                    toolHiddenMap[drawState] = false;
                }
                setCustomToolbarToolHiddenMap(
                    settings[AppSettingsGroup.Screenshot].toolbarHiddenToolList.reduce(
                        (acc, drawState) => {
                            acc[drawState] = true;
                            return acc;
                        },
                        toolHiddenMap,
                    ),
                );
            },
            [setCustomToolbarToolHiddenMap, setEnableLockDrawTool, setShowLockDrawTool],
        ),
    );
    const draggingRef = useRef(false);

    const updateEnableKeyEvent = useCallback(() => {
        setEnableKeyEvent(enableRef.current && !draggingRef.current);
    }, [setEnableKeyEvent]);

    const onDraggingChange = useCallback(
        (dragging: boolean) => {
            draggingRef.current = dragging;
            updateEnableKeyEvent();
        },
        [updateEnableKeyEvent],
    );

    const setDragging = useCallback(
        (dragging: boolean) => {
            if (draggingRef.current === dragging) {
                return;
            }

            onDraggingChange(dragging);
        },
        [onDraggingChange],
    );

    const { isReadyStatus, isReady } = usePluginService();
    const onToolClick = useCallback(
        (drawState: DrawState) => {
            const prev = getDrawState();

            if (drawState === DrawState.ScrollScreenshot) {
                const selectRect = selectLayerActionRef.current?.getSelectRect();

                if (!selectRect) {
                    message.error(intl.formatMessage({ id: 'draw.scrollScreenshot.limitTip' }));
                    return;
                }

                const minSize = Math.min(
                    selectRect.max_x - selectRect.min_x,
                    selectRect.max_y - selectRect.min_y,
                );
                if (minSize < 200) {
                    message.error(intl.formatMessage({ id: 'draw.scrollScreenshot.limitTip' }));
                    return;
                } else if (minSize < 300) {
                    message.warning(
                        intl.formatMessage({ id: 'draw.scrollScreenshot.limitTip.warning' }),
                    );
                }
            }

            if (drawState === DrawState.Lock) {
                updateAppSettings(
                    AppSettingsGroup.Cache,
                    { enableLockDrawTool: !enableLockDrawToolRef.current },
                    true,
                    true,
                    false,
                    true,
                    false,
                );

                return;
            }

            if (prev === DrawState.Text) {
                // 判断是否有未完成编辑的文本
                const appState = drawCacheLayerActionRef.current?.getAppState();
                if (appState?.editingTextElement) {
                    // 将点击事件传递给 excalidraw，停止文本编辑
                    const canvas = getExcalidrawCanvas();
                    canvas?.dispatchEvent(new PointerEvent('pointerdown'));
                }
            }

            let next = drawState;
            if (prev === drawState && prev !== DrawState.Idle) {
                if (drawState === DrawState.ScrollScreenshot) {
                    next = DrawState.Idle;
                } else {
                    next = DrawState.Select;
                }
            }

            if (next !== DrawState.Idle) {
                setCaptureStep(CaptureStep.Draw);
            } else {
                setCaptureStep(CaptureStep.Select);
            }

            let toolLocked = true;
            if (showLockDrawToolRef.current) {
                toolLocked = enableLockDrawToolRef.current;
            }

            if (next === DrawState.Select || next === DrawState.Idle) {
                // 取消正在编辑中的元素
                drawCacheLayerActionRef.current?.finishDraw();
            }

            switch (next) {
                case DrawState.Idle:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'hand',
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Select:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'selection',
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Rect:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'rectangle',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Diamond:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'diamond',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Ellipse:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'ellipse',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Arrow:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'arrow',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Line:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'line',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Pen:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'freedraw',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Text:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'text',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.SerialNumber:
                    break;
                case DrawState.Blur:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'blur',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.BlurFreeDraw:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'blur_freedraw',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Watermark:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'watermark',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Highlight:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'highlight',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.Eraser:
                    drawCacheLayerActionRef.current?.setActiveTool(
                        {
                            type: 'eraser',
                            locked: toolLocked,
                        },
                        undefined,
                        next,
                    );
                    break;
                case DrawState.OcrDetect:
                case DrawState.OcrTranslate:
                    if (isReady?.(PLUGIN_ID_RAPID_OCR)) {
                        onOcrDetect();
                    }
                    break;
                case DrawState.VideoRecord:
                case DrawState.ScanQrcode:
                case DrawState.ExtraTools:
                    break;
                default:
                    break;
            }

            if (next === DrawState.ScrollScreenshot) {
                setEnableScrollScreenshot(true);
            } else {
                setEnableScrollScreenshot(false);
            }

            setDrawState(next);
        },
        [
            drawCacheLayerActionRef,
            enableLockDrawToolRef,
            getDrawState,
            intl,
            isReady,
            message,
            onOcrDetect,
            selectLayerActionRef,
            setCaptureStep,
            setDrawState,
            showLockDrawToolRef,
            updateAppSettings,
        ],
    );

    useStateSubscriber(
        ExcalidrawEventPublisher,
        useCallback(
            (params: ExcalidrawEventParams | undefined) => {
                if (!enableRef.current) {
                    return;
                }

                if (params?.event === 'onChange') {
                    // 非锁定状态自动回退到选择状态
                    if (
                        params.params.appState.activeTool.type === 'selection' &&
                        getDrawState() !== DrawState.Select &&
                        getDrawState() !== DrawState.Idle &&
                        isDrawTool(getDrawState())
                    ) {
                        onToolClick(DrawState.Select);
                    }
                }
            },
            [getDrawState, onToolClick],
        ),
    );

    const drawToolbarContextValue = useMemo(() => {
        return {
            drawToolarContainerRef,
            drawToolbarRef,
            draggingRef,
            setDragging,
        };
    }, [drawToolarContainerRef, drawToolbarRef, draggingRef, setDragging]);

    const canHandleScreenshotTypeRef = useRef(false);
    useStateSubscriber(
        CaptureEventPublisher,
        useCallback(
            (event: CaptureEventParams | undefined) => {
                if (!event) {
                    return;
                }

                if (event.event === CaptureEvent.onCaptureReady) {
                    canHandleScreenshotTypeRef.current = true;
                }

                if (drawToolarContainerRef.current) {
                    if (event.event === CaptureEvent.onCaptureFinish) {
                        drawToolarContainerRef.current.style.opacity = '0';
                    } else if (event.event === CaptureEvent.onCaptureLoad) {
                        drawToolarContainerRef.current.style.opacity = '1';
                    }
                }
            },
            [drawToolarContainerRef],
        ),
    );

    const showDrawToolbarContainer = useCallback(() => {
        if (drawToolbarOpacityWrapRef.current) {
            drawToolbarOpacityWrapRef.current.style.transition = `opacity ${token.motionDurationMid} ${token.motionEaseInOut}`;
            drawToolbarOpacityWrapRef.current.style.opacity = '1';
        }

        const subToolContainer =
            scrollScreenshotToolActionRef.current?.getScrollScreenshotSubToolContainer();

        if (subToolContainer) {
            subToolContainer.style.transition = `opacity ${token.motionDurationMid} ${token.motionEaseInOut}`;
            subToolContainer.style.opacity = '1';
        }
    }, [token.motionDurationMid, token.motionEaseInOut]);
    const showDrawToolbarContainerDebounce = useMemo(
        () => debounce(showDrawToolbarContainer, 512),
        [showDrawToolbarContainer],
    );

    const [, setDrawEvent] = useStateSubscriber(DrawEventPublisher, undefined);
    const publishSelectRectAnimationParams = useCallback(() => {
        setDrawEvent({
            event: DrawEvent.SelectRectParamsAnimationChange,
            params: {
                selectRectParams: selectLayerActionRef.current!.getSelectRectParams()!,
            },
        });
        setDrawEvent(undefined);
    }, [setDrawEvent, selectLayerActionRef]);

    const onEnableChange = useCallback(
        (enable: boolean) => {
            enableRef.current = enable;
            dragButtonActionRef.current?.setEnable(enable);

            if (canHandleScreenshotTypeRef.current) {
                switch (getScreenshotType()?.type) {
                    case ScreenshotType.Fixed:
                        onFixed();
                        break;
                    case ScreenshotType.OcrDetect:
                        onToolClick(DrawState.OcrDetect);
                        break;
                    case ScreenshotType.OcrTranslate:
                        onToolClick(DrawState.OcrTranslate);
                        break;
                    case ScreenshotType.Copy:
                        onCopyToClipboard();
                        break;
                    case ScreenshotType.VideoRecord:
                        onToolClick(DrawState.VideoRecord);
                        break;
                    case ScreenshotType.TopWindow:
                        onTopWindow();
                        break;
                    case ScreenshotType.Default:
                    default:
                        onToolClick(DrawState.Idle);
                        break;
                }
                canHandleScreenshotTypeRef.current = false;
            }

            // 重置下工具栏样式，防止滚动截图时直接结束截图
            if (enable) {
                showDrawToolbarContainerDebounce();
                publishSelectRectAnimationParams();
            }
        },
        [
            getScreenshotType,
            onCopyToClipboard,
            onFixed,
            onToolClick,
            onTopWindow,
            showDrawToolbarContainerDebounce,
            publishSelectRectAnimationParams,
        ],
    );

    const setEnable = useCallback(
        (enable: boolean) => {
            if (enableRef.current === enable) {
                return;
            }

            onEnableChange(enable);
            updateEnableKeyEvent();
        },
        [onEnableChange, updateEnableKeyEvent],
    );

    useImperativeHandle(actionRef, () => {
        return {
            setEnable,
            onToolClick,
        };
    }, [onToolClick, setEnable]);

    const disableNormalScreenshotTool = enableScrollScreenshot;

    useStateSubscriber(
        DrawEventPublisher,
        useCallback(
            (event: DrawEventParams | undefined) => {
                const subToolContainer =
                    scrollScreenshotToolActionRef.current?.getScrollScreenshotSubToolContainer();
                if (!drawToolbarOpacityWrapRef.current || !subToolContainer) {
                    return;
                }

                if (event?.event === DrawEvent.ScrollScreenshot) {
                    subToolContainer.style.transition = 'unset';
                    subToolContainer.style.opacity = '0';
                    drawToolbarOpacityWrapRef.current.style.transition =
                        subToolContainer.style.transition;
                    drawToolbarOpacityWrapRef.current.style.opacity =
                        subToolContainer.style.opacity;
                    showDrawToolbarContainerDebounce();
                }
            },
            [showDrawToolbarContainerDebounce],
        ),
    );

    const handleMouseDown = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
        e.stopPropagation();
        e.preventDefault();
    }, []);
    const handleContextMenu = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
        e.stopPropagation();
        e.preventDefault();
    }, []);
    const handleMouseEnter = useCallback(() => {
        setDrawToolbarState({ ...getDrawToolbarState(), mouseHover: true });
    }, [getDrawToolbarState, setDrawToolbarState]);
    const handleMouseLeave = useCallback(() => {
        setDrawToolbarState({ ...getDrawToolbarState(), mouseHover: false });
    }, [getDrawToolbarState, setDrawToolbarState]);
    const handleDoubleClick = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
        e.stopPropagation();
        e.preventDefault();
    }, []);

    return (
        <div
            className="draw-toolbar-container"
            onMouseDown={handleMouseDown}
            onContextMenu={handleContextMenu}
            ref={drawToolarContainerRef}
        >
            <DrawToolbarContext.Provider value={drawToolbarContextValue}>
                <div ref={drawToolbarOpacityWrapRef}>
                    <div
                        onMouseEnter={handleMouseEnter}
                        onMouseLeave={handleMouseLeave}
                        className="draw-toolbar"
                        onDoubleClick={handleDoubleClick}
                        ref={drawToolbarRef}
                    >
                        <Flex align="center" gap={token.paddingXS} className="draw-toolbar-content">
                            <DragButton actionRef={dragButtonActionRef} />

                            {/* 默认状态 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Idle]}
                                componentKey={KeyEventKey.MoveTool}
                                icon={<DragOutlined />}
                                drawState={DrawState.Idle}
                                onClick={() => {
                                    onToolClick(DrawState.Idle);
                                }}
                            />

                            {/* 选择状态 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Select]}
                                componentKey={KeyEventKey.SelectTool}
                                icon={<ArrowSelectIcon style={{ fontSize: '1.08em' }} />}
                                drawState={DrawState.Select}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Select);
                                }}
                            />

                            {showLockDrawTool && (
                                <>
                                    {/* 锁定绘制工具 */}
                                    <ToolButton
                                        hidden={customToolbarToolHiddenMap?.[DrawState.Lock]}
                                        componentKey={KeyEventKey.LockDrawTool}
                                        icon={<LockOutlined />}
                                        drawState={DrawState.Lock}
                                        enableState={enableLockDrawTool}
                                        onClick={() => {
                                            onToolClick(DrawState.Lock);
                                        }}
                                    />
                                </>
                            )}

                            <div className="draw-toolbar-splitter" />

                            {/* 矩形 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Rect]}
                                componentKey={KeyEventKey.RectTool}
                                icon={<RectIcon style={{ fontSize: '1em' }} />}
                                disable={disableNormalScreenshotTool}
                                extraDrawState={[DrawState.Diamond]}
                                drawState={DrawState.Rect}
                                onClick={() => {
                                    onToolClick(DrawState.Rect);
                                }}
                            />

                            {/* 椭圆 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Ellipse]}
                                componentKey={KeyEventKey.EllipseTool}
                                icon={<CircleIcon style={{ fontSize: '1em' }} />}
                                drawState={DrawState.Ellipse}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Ellipse);
                                }}
                            />

                            {/* 箭头 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Arrow]}
                                componentKey={KeyEventKey.ArrowTool}
                                icon={<ArrowIcon style={{ fontSize: '0.83em' }} />}
                                drawState={DrawState.Arrow}
                                extraDrawState={[DrawState.Line]}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Arrow);
                                }}
                            />

                            {/* 画笔 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Pen]}
                                componentKey={KeyEventKey.PenTool}
                                icon={<PenIcon style={{ fontSize: '1.08em' }} />}
                                drawState={DrawState.Pen}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Pen);
                                }}
                            />

                            {/* 文本 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Text]}
                                componentKey={KeyEventKey.TextTool}
                                icon={<TextIcon style={{ fontSize: '1.08em' }} />}
                                drawState={DrawState.Text}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Text);
                                }}
                            />

                            {/* 序列号 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.SerialNumber]}
                                componentKey={KeyEventKey.SerialNumberTool}
                                icon={<SerialNumberIcon style={{ fontSize: '1.16em' }} />}
                                drawState={DrawState.SerialNumber}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.SerialNumber);
                                }}
                            />

                            {/* 模糊 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Blur]}
                                componentKey={KeyEventKey.BlurTool}
                                extraDrawState={[DrawState.BlurFreeDraw]}
                                icon={<FilterIcon />}
                                drawState={DrawState.Blur}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Blur);
                                }}
                            />

                            {/* 橡皮擦 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Eraser]}
                                componentKey={KeyEventKey.EraserTool}
                                icon={<EraserIcon style={{ fontSize: '0.9em' }} />}
                                drawState={DrawState.Eraser}
                                disable={disableNormalScreenshotTool}
                                onClick={() => {
                                    onToolClick(DrawState.Eraser);
                                }}
                            />

                            <DrawExtraTool
                                customToolbarToolHiddenMap={customToolbarToolHiddenMap}
                                onToolClickAction={onToolClick}
                            />

                            {!customToolbarToolHiddenMap?.[DrawState.Redo] && (
                                <div className="draw-toolbar-splitter" />
                            )}

                            <HistoryControls
                                hidden={customToolbarToolHiddenMap?.[DrawState.Redo]}
                                disable={enableScrollScreenshot}
                            />

                            <div className="draw-toolbar-splitter" />

                            <ExtraTool onToolClickAction={onToolClick} />

                            {/* 固定到屏幕 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Fixed]}
                                componentKey={KeyEventKey.FixedTool}
                                icon={
                                    <FixedIcon
                                        style={{
                                            fontSize: '1.15em',
                                            position: 'relative',
                                            bottom: '0.02em',
                                        }}
                                    />
                                }
                                drawState={DrawState.Fixed}
                                onClick={() => {
                                    onFixed();
                                }}
                            />

                            {/* OCR */}
                            <ToolButton
                                hidden={
                                    customToolbarToolHiddenMap?.[DrawState.OcrDetect] ||
                                    !isReadyStatus?.(PLUGIN_ID_RAPID_OCR)
                                }
                                componentKey={KeyEventKey.OcrDetectTool}
                                icon={<OcrDetectIcon style={{ fontSize: '0.88em' }} />}
                                drawState={DrawState.OcrDetect}
                                disable={
                                    disableNormalScreenshotTool ||
                                    !isReadyStatus?.(PLUGIN_ID_RAPID_OCR)
                                }
                                onClick={() => {
                                    onToolClick(DrawState.OcrDetect);
                                }}
                            />

                            {/* OCR 翻译 */}
                            <ToolButton
                                hidden={
                                    customToolbarToolHiddenMap?.[DrawState.OcrTranslate] ||
                                    !isReadyStatus?.(PLUGIN_ID_RAPID_OCR)
                                }
                                componentKey={KeyEventKey.OcrTranslateTool}
                                icon={<OcrTranslateIcon style={{ fontSize: '1em' }} />}
                                drawState={DrawState.OcrTranslate}
                                disable={
                                    disableNormalScreenshotTool ||
                                    !isReadyStatus?.(PLUGIN_ID_RAPID_OCR)
                                }
                                onClick={() => {
                                    onToolClick(DrawState.OcrTranslate);
                                }}
                            />

                            {/* 滚动截图 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.ScrollScreenshot]}
                                componentKey={KeyEventKey.ScrollScreenshotTool}
                                icon={
                                    <div style={{ position: 'relative', top: '0.11em' }}>
                                        <ScrollScreenshotIcon style={{ fontSize: '1.2em' }} />
                                    </div>
                                }
                                drawState={DrawState.ScrollScreenshot}
                                onClick={() => {
                                    onToolClick(DrawState.ScrollScreenshot);
                                }}
                            />

                            {/* 快速保存截图 */}
                            {enableFastSave && (
                                <ToolButton
                                    hidden={customToolbarToolHiddenMap?.[DrawState.FastSave]}
                                    componentKey={KeyEventKey.FastSaveTool}
                                    icon={<FastSaveIcon style={{ fontSize: '1.08em' }} />}
                                    drawState={DrawState.FastSave}
                                    onClick={() => {
                                        onSave(true);
                                    }}
                                />
                            )}

                            {/* 保存截图 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Save]}
                                componentKey={KeyEventKey.SaveTool}
                                icon={<SaveIcon style={{ fontSize: '1em' }} />}
                                drawState={DrawState.Save}
                                onClick={() => {
                                    onSave();
                                }}
                            />

                            <div className="draw-toolbar-splitter" />

                            {/* 取消截图 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Cancel]}
                                componentKey={KeyEventKey.CancelTool}
                                icon={
                                    <CloseOutlined
                                        style={{ fontSize: '0.83em', color: token.colorError }}
                                    />
                                }
                                confirmTip={
                                    shortcutCanleTip ? (
                                        <FormattedMessage id="draw.cancel.tip1" />
                                    ) : undefined
                                }
                                drawState={DrawState.Cancel}
                                onClick={() => {
                                    onCancel();
                                }}
                            />

                            {/* 复制截图 */}
                            <ToolButton
                                hidden={customToolbarToolHiddenMap?.[DrawState.Copy]}
                                componentKey={KeyEventKey.CopyTool}
                                icon={
                                    <CopyOutlined
                                        style={{ fontSize: '0.92em', color: token.colorPrimary }}
                                    />
                                }
                                drawState={DrawState.Copy}
                                onClick={() => {
                                    onCopyToClipboard();
                                }}
                            />
                        </Flex>
                    </div>
                </div>

                <BlurTool />
                <HighlightTool />
                <ScrollScreenshot actionRef={scrollScreenshotToolActionRef} />
            </DrawToolbarContext.Provider>
            <style jsx>{`
                .draw-toolbar-container {
                    pointer-events: none;
                    user-select: none;
                    position: absolute;
                    z-index: ${zIndexs.Draw_Toolbar};
                    top: 0;
                    left: 0;
                }

                .draw-toolbar-container:hover {
                    z-index: ${zIndexs.Draw_ToolbarHover};
                }

                .draw-toolbar {
                    position: absolute;
                    opacity: 0;
                    transform-origin: top left;
                }

                :global(.draw-toolbar-content > div) {
                    line-height: 0;
                }

                .draw-toolbar {
                    padding: ${token.paddingXXS}px ${token.paddingSM}px;
                    box-sizing: border-box;
                    background-color: ${token.colorBgContainer};
                    border-radius: ${token.borderRadiusLG}px;
                    cursor: default; /* 防止非拖动区域也变成可拖动状态 */
                    color: ${token.colorText};
                    box-shadow: 0 0 3px 0px ${token.colorPrimaryHover};
                    transition: opacity ${token.motionDurationMid} ${token.motionEaseInOut};
                    z-index: ${zIndexs.Draw_Toolbar};
                }

                .draw-subtoolbar {
                    opacity: 0;
                }

                .draw-subtoolbar-container {
                    position: absolute;
                    right: 0;
                    bottom: calc(-100% - ${token.marginXXS}px);
                    height: 100%;
                }

                :global(.drag-button) {
                    color: ${token.colorTextQuaternary};
                    cursor: move;
                }

                .draw-toolbar :global(.draw-toolbar-drag) {
                    font-size: 18px;
                    margin-right: -3px;
                    margin-left: -3px;
                }

                :global(.popover-toolbar) :global(.ant-btn) :global(.ant-btn-icon),
                .draw-toolbar-container :global(.ant-btn) :global(.ant-btn-icon) {
                    font-size: 24px;
                    display: flex;
                    align-items: center;
                }

                .draw-toolbar-container :global(.ant-btn-icon) {
                    display: flex;
                    align-items: center;
                }

                .draw-toolbar-container :global(.draw-toolbar-splitter),
                .draw-toolbar-splitter {
                    width: 1px;
                    height: 0.83em;
                    background-color: ${token.colorBorder};
                    margin: 0 ${token.marginXXS}px;
                }
            `}</style>
        </div>
    );
};

export const DrawToolbar = React.memo(DrawToolbarCore);
