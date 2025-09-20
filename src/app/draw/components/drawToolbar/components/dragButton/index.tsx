import { DrawContext } from '@/app/draw/types';
import { useCallbackRender } from '@/hooks/useCallbackRender';
import { MousePosition } from '@/utils/mousePosition';
import { HolderOutlined } from '@ant-design/icons';
import { theme } from 'antd';
import React, {
    useCallback,
    useContext,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
} from 'react';
import { useIntl } from 'react-intl';
import { DrawToolbarContext, isEnableSubToolbar } from '../../extra';
import { ElementRect } from '@/commands';
import { updateElementPosition, UpdateElementPositionResult } from './extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useMonitorRect } from '../../../statusBar';
import { ElementDraggingPublisher } from '@/app/draw/extra';

export type DragButtonActionType = {
    setEnable: (enable: boolean) => void;
};

const useDragElementCore: () => {
    update: (
        element: HTMLElement,
        baseOffsetX: number,
        baseOffsetY: number,
        contentScale?: number,
        calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
    ) => UpdateElementPositionResult;
    reset: () => void;
    mouseOriginPositionRef: React.RefObject<MousePosition>;
    mouseCurrentPositionRef: React.RefObject<MousePosition>;
    toolbarCurrentRectRef: React.RefObject<ElementRect>;
    toolbarPreviousRectRef: React.RefObject<ElementRect | undefined>;
    applyDragResult: (dragRes: UpdateElementPositionResult) => void;
} = () => {
    // 保存 toolbar 位置
    const mouseOriginPositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const mouseCurrentPositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const toolbarCurrentRectRef = useRef<ElementRect>({
        min_x: 0,
        min_y: 0,
        max_x: 0,
        max_y: 0,
    });
    const toolbarPreviousRectRef = useRef<ElementRect>(undefined);
    const update = useCallback(
        (
            element: HTMLElement,
            baseOffsetX: number,
            baseOffsetY: number,
            contentScale?: number,
            calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
        ): UpdateElementPositionResult => {
            const dragRes = updateElementPosition(
                element,
                baseOffsetX,
                baseOffsetY,
                mouseOriginPositionRef.current,
                mouseCurrentPositionRef.current,
                toolbarPreviousRectRef.current,
                undefined,
                contentScale,
                calculatedBoundaryRect,
            );

            return dragRes;
        },
        [],
    );

    const applyDragResult = useCallback((dragRes: UpdateElementPositionResult) => {
        toolbarCurrentRectRef.current = dragRes.rect;
        mouseOriginPositionRef.current = dragRes.originPosition;
    }, []);

    const reset = useCallback(() => {
        mouseOriginPositionRef.current = new MousePosition(0, 0);
        mouseCurrentPositionRef.current = new MousePosition(0, 0);
        toolbarCurrentRectRef.current = {
            min_x: 0,
            min_y: 0,
            max_x: 0,
            max_y: 0,
        };
        toolbarPreviousRectRef.current = undefined;
    }, []);

    return useMemo(() => {
        return {
            update,
            applyDragResult,
            reset,
            mouseOriginPositionRef,
            mouseCurrentPositionRef,
            toolbarCurrentRectRef,
            toolbarPreviousRectRef,
        };
    }, [reset, update, applyDragResult]);
};

export type DragElementConfig = {
    getBaseOffset: (element: HTMLElement) => {
        x: number;
        y: number;
    };
    getContentScale?: () => number;
    calculatedBoundaryRect?: (rect: ElementRect) => ElementRect;
};

export type DragElementOptionalConfig = {
    config: DragElementConfig;
    /** 判断结果是否可以应用 */
    canApply: (dragRes: UpdateElementPositionResult) => boolean;
    /** 判断是否需要尝试使用当前配置 */
    needTry: (mainDragRes: UpdateElementPositionResult) => boolean;
};

export const useDragElement = (
    mainConfig: DragElementConfig,
    optionalConfigs?: DragElementOptionalConfig[],
): {
    update: (
        element: HTMLElement,
        contentScale?: number,
        calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
    ) => UpdateElementPositionResult;
    resetConfig: () => void;
    resetDrag: () => void;
    onMouseDown: (event: React.MouseEvent<HTMLDivElement> | MouseEvent) => void;
    onMouseMove: (
        event: React.MouseEvent<HTMLDivElement> | MouseEvent,
        element: HTMLElement,
        contentScale?: number,
        calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
    ) => void;
    onMouseUp: () => void;
} => {
    const {
        update: updateCore,
        reset: resetDrag,
        mouseOriginPositionRef,
        mouseCurrentPositionRef,
        toolbarPreviousRectRef,
        toolbarCurrentRectRef,
        applyDragResult,
    } = useDragElementCore();

    const draggingRef = useRef(false);
    const [, setDragging] = useStateSubscriber(ElementDraggingPublisher, undefined);

    const selectedConfigRef = useRef<DragElementConfig | undefined>(undefined);
    const update = useCallback(
        (
            element: HTMLElement,
            contentScale?: number,
            calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
        ) => {
            const baseOffset = selectedConfigRef.current
                ? selectedConfigRef.current.getBaseOffset(element)
                : mainConfig.getBaseOffset(element);
            let dragRes = updateCore(
                element,
                baseOffset.x,
                baseOffset.y,
                contentScale,
                calculatedBoundaryRect,
            );

            if (!selectedConfigRef.current) {
                for (const { config, needTry, canApply } of optionalConfigs ?? []) {
                    if (!needTry(dragRes)) {
                        continue;
                    }

                    const baseOffset = config.getBaseOffset(element);
                    const tempDragRes = updateCore(
                        element,
                        baseOffset.x,
                        baseOffset.y,
                        contentScale,
                        calculatedBoundaryRect,
                    );
                    if (canApply(tempDragRes)) {
                        dragRes = tempDragRes;
                        selectedConfigRef.current = config;
                        break;
                    }
                }

                if (!selectedConfigRef.current) {
                    // 回退到原来的方案
                    const baseOffset = mainConfig.getBaseOffset(element);
                    dragRes = updateCore(
                        element,
                        baseOffset.x,
                        baseOffset.y,
                        contentScale,
                        calculatedBoundaryRect,
                    );
                    selectedConfigRef.current = mainConfig;
                }
            }

            applyDragResult(dragRes);

            return dragRes;
        },
        [mainConfig, optionalConfigs, updateCore, applyDragResult],
    );
    const updateRender = useCallbackRender(update);

    const resetConfig = useCallback(() => {
        selectedConfigRef.current = undefined;
    }, []);

    const onMouseDown = useCallback(
        (event: React.MouseEvent<HTMLDivElement> | MouseEvent) => {
            draggingRef.current = true;
            setDragging(true);
            mouseOriginPositionRef.current = new MousePosition(event.clientX, event.clientY);
            mouseCurrentPositionRef.current = new MousePosition(event.clientX, event.clientY);
            toolbarPreviousRectRef.current = toolbarCurrentRectRef.current;
        },
        [
            mouseOriginPositionRef,
            mouseCurrentPositionRef,
            toolbarPreviousRectRef,
            toolbarCurrentRectRef,
            draggingRef,
            setDragging,
        ],
    );
    const onMouseMoveCore = useCallback(
        (
            event: React.MouseEvent<HTMLDivElement> | MouseEvent,
            element: HTMLElement,
            contentScale?: number,
            calculatedBoundaryRect?: (rect: ElementRect) => ElementRect,
        ) => {
            if (!draggingRef.current) {
                return;
            }

            mouseCurrentPositionRef.current = new MousePosition(event.clientX, event.clientY);
            updateRender(element, contentScale, calculatedBoundaryRect);
        },
        [draggingRef, mouseCurrentPositionRef, updateRender],
    );
    const onMouseUp = useCallback(() => {
        if (!draggingRef.current) {
            return;
        }

        draggingRef.current = false;
        setDragging(false);
    }, [draggingRef, setDragging]);

    return useMemo(() => {
        return {
            update: updateRender,
            resetConfig,
            resetDrag,
            onMouseDown,
            onMouseMove: onMouseMoveCore,
            onMouseUp,
        };
    }, [onMouseDown, onMouseMoveCore, onMouseUp, resetConfig, resetDrag, updateRender]);
};

const DragButtonCore: React.FC<{
    actionRef: React.RefObject<DragButtonActionType | undefined>;
}> = ({ actionRef }) => {
    const enableRef = useRef(false);

    const enableSubToolbarRef = useRef(false);

    const { selectLayerActionRef } = useContext(DrawContext);
    const { drawToolbarRef, setDragging, draggingRef, drawToolarContainerRef } =
        useContext(DrawToolbarContext);
    const { token } = theme.useToken();

    const {
        contentScale: [, , contentScaleRef],
        calculatedBoundaryRect,
    } = useMonitorRect(true);

    const getSelectedRect = useCallback(() => {
        return (
            selectLayerActionRef.current?.getSelectRect() ?? {
                min_x: 0,
                min_y: 0,
                max_x: 0,
                max_y: 0,
            }
        );
    }, [selectLayerActionRef]);
    const {
        update: updateDrawToolbarStyleCore,
        resetConfig,
        resetDrag,
        onMouseDown,
        onMouseMove,
        onMouseUp,
    } = useDragElement(
        useMemo(() => {
            return {
                getBaseOffset: (element: HTMLElement) => {
                    const selectedRect = getSelectedRect();

                    return {
                        x:
                            selectedRect.max_x / window.devicePixelRatio -
                            element.clientWidth * contentScaleRef.current,
                        y:
                            selectedRect.max_y / window.devicePixelRatio +
                            token.marginXXS * contentScaleRef.current,
                    };
                },
            };
        }, [contentScaleRef, getSelectedRect, token.marginXXS]),
        useMemo(() => {
            return [
                {
                    config: {
                        getBaseOffset: (element: HTMLElement) => {
                            const selectedRect = getSelectedRect();

                            return {
                                x:
                                    selectedRect.max_x / window.devicePixelRatio -
                                    element.clientWidth * contentScaleRef.current,
                                y:
                                    selectedRect.min_y / window.devicePixelRatio -
                                    element.clientHeight * contentScaleRef.current -
                                    token.marginXXS * contentScaleRef.current,
                            };
                        },
                    },
                    needTry: (dragRes: UpdateElementPositionResult) => {
                        return dragRes.isBeyondMaxY;
                    },
                    canApply: (dragRes: UpdateElementPositionResult) => {
                        return !(dragRes.isBeyondMaxY || dragRes.isBeyondMinY);
                    },
                },
            ];
        }, [contentScaleRef, getSelectedRect, token.marginXXS]),
    );

    const updateDrawToolbarStyle = useCallback(() => {
        const drawToolbar = drawToolbarRef.current;
        if (!drawToolbar) {
            return;
        }

        updateDrawToolbarStyleCore(drawToolbar, contentScaleRef.current, calculatedBoundaryRect);
    }, [drawToolbarRef, updateDrawToolbarStyleCore, contentScaleRef, calculatedBoundaryRect]);

    const handleMouseDown = useCallback(
        (e: React.MouseEvent<HTMLDivElement>) => {
            e.stopPropagation();
            setDragging(true);
            onMouseDown(e);
        },
        [onMouseDown, setDragging],
    );

    // 处理鼠标释放事件
    const handleMouseUp = useCallback(() => {
        if (!draggingRef.current) {
            return;
        }

        setDragging(false);
        onMouseUp();
    }, [draggingRef, setDragging, onMouseUp]);

    // 处理鼠标移动事件
    const handleMouseMove = useCallback(
        (event: React.MouseEvent<HTMLDivElement> | MouseEvent) => {
            if (!draggingRef.current || !drawToolbarRef.current) {
                return;
            }

            onMouseMove(
                event,
                drawToolbarRef.current,
                contentScaleRef.current,
                calculatedBoundaryRect,
            );
        },
        [calculatedBoundaryRect, contentScaleRef, draggingRef, drawToolbarRef, onMouseMove],
    );

    useEffect(() => {
        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
        };
    }, [handleMouseMove, handleMouseUp]);

    const onEnableChange = useCallback(
        (enable: boolean) => {
            enableRef.current = enable;

            if (enable) {
                drawToolarContainerRef.current!.style.opacity = '1';
                drawToolbarRef.current!.style.opacity = '1';
                drawToolbarRef.current!.style.pointerEvents = 'auto';

                // 重置偏移，避免工具栏定位受超出边界的逻辑影响
                resetDrag();
                updateDrawToolbarStyle();
            } else {
                // drawToolbarRef 设置 opacity 会有过渡效果，这里直接把 container 设置为 0 避免过渡效果
                drawToolarContainerRef.current!.style.opacity = '0';
                drawToolbarRef.current!.style.opacity = '0';
                drawToolbarRef.current!.style.pointerEvents = 'none';
                resetConfig();
                resetDrag();
            }
        },
        [drawToolarContainerRef, drawToolbarRef, resetConfig, resetDrag, updateDrawToolbarStyle],
    );

    const setEnable = useCallback(
        (enable: boolean) => {
            if (enableRef.current === enable) {
                return;
            }

            onEnableChange(enable);
        },
        [onEnableChange],
    );

    const onDrawStateChange = useCallback(
        (drawState: DrawState) => {
            enableSubToolbarRef.current = isEnableSubToolbar(drawState);

            updateDrawToolbarStyle();
        },
        [updateDrawToolbarStyle],
    );
    useStateSubscriber(DrawStatePublisher, onDrawStateChange);

    useImperativeHandle(actionRef, () => {
        return {
            setEnable,
        };
    }, [setEnable]);

    const intl = useIntl();
    const dragTitle = useMemo(() => {
        return intl.formatMessage({ id: 'draw.drag' });
    }, [intl]);

    return (
        <div
            className="draw-toolbar-drag drag-button"
            title={dragTitle}
            onMouseDown={handleMouseDown}
        >
            <HolderOutlined />
        </div>
    );
};

export const DragButton = React.memo(DragButtonCore);
