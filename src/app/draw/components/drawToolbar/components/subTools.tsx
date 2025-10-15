import { HolderOutlined } from '@ant-design/icons';
import { Flex, theme } from 'antd';
import { createStyles } from 'antd-style';
import { useCallback, useContext, useEffect, useImperativeHandle, useMemo, useRef } from 'react';
import { useIntl } from 'react-intl';
import { DrawContext } from '@/app/draw/types';
import { zIndexs } from '@/utils/zIndex';
import { useMonitorRect } from '../../statusBar';
import { useDragElement } from './dragButton';

export type SubToolsActionType = {
    getSubToolContainer: () => HTMLDivElement | null;
};

const useStyles = createStyles(({ token }) => ({
    subToolsContainer: {
        position: 'fixed',
        zIndex: zIndexs.Draw_SubToolbar,
        top: 0,
        left: 0,
    },
    subTools: {
        opacity: 0,
        pointerEvents: 'auto',
        padding: `${token.paddingSM}px ${token.paddingXXS}px`,
        boxSizing: 'border-box',
        backgroundColor: token.colorBgContainer,
        borderRadius: `${token.borderRadiusLG}px`,
        cursor: 'default', // 防止非拖动区域也变成可拖动状态
        color: token.colorText,
        boxShadow: `0 0 3px 0px ${token.colorPrimaryHover}`,
        transition: `opacity ${token.motionDurationMid} ${token.motionEaseInOut}`,
        transformOrigin: 'top left',
    },
    dragButton: {
        marginTop: `${-token.marginXXS / 2}px`,
        transform: 'rotate(90deg)',
        fontSize: '18px',
        color: token.colorTextQuaternary,
        cursor: 'move',
    },
    antBtnIcon: {
        '& .ant-btn .ant-btn-icon': {
            fontSize: '24px',
        },
    },
}));

export const SubTools: React.FC<{
    buttons: React.ReactNode[];
    actionRef?: React.RefObject<SubToolsActionType | undefined>;
}> = ({ buttons, actionRef }) => {
    const intl = useIntl();
    const { token } = theme.useToken();
    const { styles } = useStyles();

    const { selectLayerActionRef } = useContext(DrawContext);

    const subToolsContainerRef = useRef<HTMLDivElement>(null);
    const subToolsRef = useRef<HTMLDivElement>(null);

    const {
        calculatedBoundaryRect,
        contentScale: [contentScale],
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
        reset: resetDrag,
        onMouseDown,
        onMouseMove,
        onMouseUp,
    } = useDragElement(
        useMemo(() => {
            return {
                getBaseOffset: (element) => {
                    const selectedRect = getSelectedRect();

                    return {
                        x:
                            selectedRect.min_x / window.devicePixelRatio -
                            (element.clientWidth + token.marginXXS) * contentScale,
                        y: selectedRect.min_y / window.devicePixelRatio,
                    };
                },
            };
        }, [contentScale, getSelectedRect, token.marginXXS]),
    );

    const updateDrawToolbarStyle = useCallback(() => {
        if (!subToolsRef.current) {
            return;
        }

        updateDrawToolbarStyleCore(subToolsRef.current, contentScale, calculatedBoundaryRect);
    }, [updateDrawToolbarStyleCore, contentScale, calculatedBoundaryRect]);

    const handleMouseDown = useCallback(
        (e: React.MouseEvent<HTMLDivElement>) => {
            e.stopPropagation();
            onMouseDown(e);
        },
        [onMouseDown],
    );

    const handleMouseMove = useCallback(
        (event: MouseEvent) => {
            if (!subToolsRef.current) return;

            onMouseMove(event, subToolsRef.current, contentScale, calculatedBoundaryRect);
        },
        [onMouseMove, contentScale, calculatedBoundaryRect],
    );

    const dragTitle = useMemo(() => {
        return intl.formatMessage({ id: 'draw.drag' });
    }, [intl]);

    useEffect(() => {
        resetDrag();
        updateDrawToolbarStyle();
        requestAnimationFrame(() => {
            if (subToolsRef.current) {
                subToolsRef.current.style.opacity = '1';
            }
        });
    }, [updateDrawToolbarStyle, resetDrag]);

    useEffect(() => {
        document.addEventListener('mousemove', handleMouseMove);
        document.addEventListener('mouseup', onMouseUp);
        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
        };
    }, [handleMouseMove, onMouseUp]);

    useImperativeHandle(
        actionRef,
        useCallback(() => {
            return {
                getSubToolContainer: () => subToolsContainerRef.current,
            };
        }, []),
    );

    return (
        <div className={styles.subToolsContainer} ref={subToolsContainerRef}>
            <div className={styles.subTools} ref={subToolsRef}>
                <div className={styles.dragButton} title={dragTitle} onMouseDown={handleMouseDown}>
                    <HolderOutlined />
                </div>
                <div className={styles.antBtnIcon}>
                    <Flex
                        align="center"
                        gap={token.paddingXS}
                        style={{ flexDirection: 'column', marginTop: -token.marginXS }}
                    >
                        {buttons}
                    </Flex>
                </div>
            </div>
        </div>
    );
};
