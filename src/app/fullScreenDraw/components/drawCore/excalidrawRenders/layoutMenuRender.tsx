import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { useCallback, useContext, useEffect, useMemo, useRef } from 'react';
import { HolderOutlined } from '@ant-design/icons';
import { useIntl } from 'react-intl';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawCoreContext, ExcalidrawEventParams, ExcalidrawEventPublisher } from '../extra';
import { useDragElement } from '@/app/draw/components/drawToolbar/components/dragButton';

const LayoutMenuRender: React.FC<{
    children: React.ReactNode;
}> = ({ children }) => {
    const intl = useIntl();

    const layoutMenuRenderRef = useRef<HTMLDivElement>(null);
    const {
        getLimitRect,
        getBaseOffset,
        getDevicePixelRatio,
        calculatedBoundaryRect,
        getDragElementOptionalConfig,
        getContentScale,
    } = useContext(DrawCoreContext);

    const getSelectedRect = useCallback(() => {
        return (
            getLimitRect() ?? {
                min_x: 0,
                min_y: 0,
                max_x: 0,
                max_y: 0,
            }
        );
    }, [getLimitRect]);
    const {
        update: updateDrawToolbarStyleCore,
        reset: resetDrag,
        onMouseDown,
        onMouseMove,
        onMouseUp,
    } = useDragElement(
        useMemo(() => {
            return {
                getBaseOffset: () => {
                    return getBaseOffset(getSelectedRect(), getDevicePixelRatio());
                },
            };
        }, [getBaseOffset, getDevicePixelRatio, getSelectedRect]),
        useMemo(() => {
            return getDragElementOptionalConfig?.(getSelectedRect(), getDevicePixelRatio());
        }, [getDevicePixelRatio, getDragElementOptionalConfig, getSelectedRect]),
    );

    const updateDrawToolbarStyle = useCallback(() => {
        const element = layoutMenuRenderRef.current;
        if (!element) {
            return;
        }

        updateDrawToolbarStyleCore(element, getContentScale?.(), calculatedBoundaryRect);
    }, [calculatedBoundaryRect, getContentScale, updateDrawToolbarStyleCore]);

    useEffect(() => {
        resetDrag();
        updateDrawToolbarStyle();
    }, [updateDrawToolbarStyle, resetDrag]);

    useStateSubscriber(
        ExcalidrawEventPublisher,
        useCallback(
            (event: ExcalidrawEventParams | undefined) => {
                if (event?.event === 'onChange') {
                    updateDrawToolbarStyle();
                }
            },
            [updateDrawToolbarStyle],
        ),
    );

    const dragTitle = useMemo(() => {
        return intl.formatMessage({ id: 'draw.drag' });
    }, [intl]);

    const handleMouseDown = useCallback(
        (e: React.MouseEvent<HTMLDivElement>) => {
            e.stopPropagation();
            onMouseDown(e);
        },
        [onMouseDown],
    );

    // 处理鼠标释放事件
    const handleMouseUp = useCallback(() => {
        onMouseUp();
    }, [onMouseUp]);

    // 处理鼠标移动事件
    const handleMouseMove = useCallback(
        (event: MouseEvent) => {
            if (!layoutMenuRenderRef.current) {
                return;
            }

            onMouseMove(
                event,
                layoutMenuRenderRef.current,
                getContentScale?.(),
                calculatedBoundaryRect,
            );
        },
        [calculatedBoundaryRect, getContentScale, onMouseMove],
    );

    useEffect(() => {
        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
        };
    }, [handleMouseMove, handleMouseUp]);

    const handleDoubleClick = useCallback<React.MouseEventHandler<HTMLDivElement>>((event) => {
        event.preventDefault();
        event.stopPropagation();
    }, []);

    return (
        <div
            ref={layoutMenuRenderRef}
            className="layout-menu-render"
            id="layout-menu-render"
            onDoubleClick={handleDoubleClick}
        >
            <div
                className="drag-button layout-menu-render-drag-button"
                title={dragTitle}
                onMouseDown={handleMouseDown}
            >
                <HolderOutlined />
                <HolderOutlined />
                <HolderOutlined />
            </div>

            {children}
        </div>
    );
};

export const layoutMenuRender: NonNullable<
    ExcalidrawPropsCustomOptions['layoutRenders']
>['menuRender'] = ({ children }) => {
    return <LayoutMenuRender>{children}</LayoutMenuRender>;
};
