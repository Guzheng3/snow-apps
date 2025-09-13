import { ElementRect } from '@/commands';
import { useCallbackRender } from '@/hooks/useCallbackRender';
import { MousePosition } from '@/utils/mousePosition';
import { HolderOutlined } from '@ant-design/icons';
import { Flex, theme } from 'antd';
import { useCallback, useContext, useEffect, useImperativeHandle, useMemo, useRef } from 'react';
import { useIntl } from 'react-intl';
import { updateElementPosition } from './dragButton/extra';
import { DrawContext } from '@/app/draw/types';
import { zIndexs } from '@/utils/zIndex';
import { useMonitorRect } from '../../statusBar';

export type SubToolsActionType = {
    getSubToolContainer: () => HTMLDivElement | null;
};

export const SubTools: React.FC<{
    buttons: React.ReactNode[];
    actionRef?: React.RefObject<SubToolsActionType | undefined>;
}> = ({ buttons, actionRef }) => {
    const intl = useIntl();
    const { token } = theme.useToken();

    const { selectLayerActionRef } = useContext(DrawContext);

    const subToolsContainerRef = useRef<HTMLDivElement>(null);
    const subToolsRef = useRef<HTMLDivElement>(null);

    const draggingRef = useRef(false);
    const mouseOriginPositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const mouseCurrentPositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const toolbarCurrentRectRef = useRef<ElementRect>({
        min_x: 0,
        min_y: 0,
        max_x: 0,
        max_y: 0,
    });
    const toolbarPreviousRectRef = useRef<ElementRect>(undefined);

    const handleMouseDown = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
        e.stopPropagation();
        draggingRef.current = true;
        mouseOriginPositionRef.current = new MousePosition(e.clientX, e.clientY);
        mouseCurrentPositionRef.current = new MousePosition(e.clientX, e.clientY);
        toolbarPreviousRectRef.current = toolbarCurrentRectRef.current;
    }, []);
    // 处理鼠标释放事件
    const handleMouseUp = useCallback(() => {
        if (!draggingRef.current) {
            return;
        }

        draggingRef.current = false;
    }, []);

    const {
        calculatedBoundaryRect,
        contentScale: [, , contentScaleRef],
    } = useMonitorRect();

    const updateDrawToolbarStyle = useCallback(() => {
        const subTools = subToolsRef.current;
        if (!subTools) {
            return;
        }

        const selectedRect = selectLayerActionRef.current?.getSelectRect();
        if (!selectedRect) {
            return;
        }

        const baseOffsetX =
            selectedRect.min_x / window.devicePixelRatio -
            (subTools.clientWidth + token.marginXXS) * contentScaleRef.current;
        const baseOffsetY = selectedRect.min_y / window.devicePixelRatio;

        const dragRes = updateElementPosition(
            subTools,
            baseOffsetX,
            baseOffsetY,
            mouseOriginPositionRef.current,
            mouseCurrentPositionRef.current,
            toolbarPreviousRectRef.current,
            undefined,
            contentScaleRef.current,
            calculatedBoundaryRect,
        );

        toolbarCurrentRectRef.current = dragRes.rect;
        mouseOriginPositionRef.current = dragRes.originPosition;
    }, [selectLayerActionRef, token.marginXXS, contentScaleRef, calculatedBoundaryRect]);

    const updateDrawToolbarStyleRender = useCallbackRender(updateDrawToolbarStyle);

    const handleMouseMove = useCallback(
        (mousePosition: MousePosition) => {
            if (!draggingRef.current) return;

            mouseCurrentPositionRef.current = mousePosition;
            updateDrawToolbarStyleRender();
        },
        [draggingRef, updateDrawToolbarStyleRender],
    );

    const dragTitle = useMemo(() => {
        return intl.formatMessage({ id: 'draw.drag' });
    }, [intl]);

    useEffect(() => {
        updateDrawToolbarStyleRender();
        requestAnimationFrame(() => {
            if (subToolsRef.current) {
                subToolsRef.current.style.opacity = '1';
            }
        });
    }, [updateDrawToolbarStyleRender]);

    useEffect(() => {
        const onMouseMove = (e: MouseEvent) => {
            const mousePosition = new MousePosition(e.clientX, e.clientY);
            handleMouseMove(mousePosition);
        };

        document.addEventListener('mousemove', onMouseMove);
        document.addEventListener('mouseup', handleMouseUp);
        return () => {
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', handleMouseUp);
        };
    }, [handleMouseMove, handleMouseUp]);

    useImperativeHandle(
        actionRef,
        useCallback(() => {
            return {
                getSubToolContainer: () => subToolsContainerRef.current,
            };
        }, []),
    );

    return (
        <div className="sub-tools-container" ref={subToolsContainerRef}>
            <div className="sub-tools" ref={subToolsRef}>
                <div className="drag-button" title={dragTitle} onMouseDown={handleMouseDown}>
                    <HolderOutlined />
                </div>
                <Flex
                    align="center"
                    gap={token.paddingXS}
                    style={{ flexDirection: 'column', marginTop: -token.marginXS }}
                >
                    {buttons}
                </Flex>
            </div>
            <style jsx>{`
                .sub-tools-container {
                    position: fixed;
                    z-index: ${zIndexs.Draw_SubToolbar};
                    top: 0;
                    left: 0;
                }

                .sub-tools {
                    opacity: 0;
                    pointer-events: auto;
                    padding: ${token.paddingSM}px ${token.paddingXXS}px;
                    box-sizing: border-box;
                    background-color: ${token.colorBgContainer};
                    border-radius: ${token.borderRadiusLG}px;
                    cursor: default; /* 防止非拖动区域也变成可拖动状态 */
                    color: ${token.colorText};
                    box-shadow: 0 0 3px 0px ${token.colorPrimaryHover};
                    transition: opacity ${token.motionDurationMid} ${token.motionEaseInOut};
                    transform-origin: top left;
                }

                .drag-button {
                    margin-top: -${token.marginXXS / 2}px;
                    transform: rotate(90deg);
                    font-size: 18px;
                }

                .sub-tools :global(.ant-btn) :global(.ant-btn-icon) {
                    font-size: 24px;
                }
            `}</style>
        </div>
    );
};
