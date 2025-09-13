import { MousePosition } from '@/utils/mousePosition';
import { zIndexs } from '@/utils/zIndex';
import { Flex, Space, theme } from 'antd';
import React, { useCallback, useImperativeHandle, useMemo, useRef, useState } from 'react';
import { updateElementPosition } from '../../../drawToolbar/components/dragButton/extra';
import { ElementRect } from '@/commands';
import {
    CaptureBoundingBoxInfo,
    CaptureEvent,
    CaptureEventPublisher,
    ScreenshotTypePublisher,
} from '@/app/draw/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { ScreenshotType } from '@/functions/screenshot';
import { debounce } from 'es-toolkit';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { RadiusIcon, ShadowIcon } from '@/components/icons';
import { useStateRef } from '@/hooks/useStateRef';
import { useCallbackRender } from '@/hooks/useCallbackRender';
import { SelectState } from '../../extra';
import { ResizeModal, ResizeModalActionType, ResizeModalParams } from './components/resizeModal';
import { useIntl } from 'react-intl';
import { AggregationColor } from 'antd/es/color-picker/color';
import { useMonitorRect } from '../../../statusBar';

export type ResizeToolbarActionType = {
    updateStyle: (selectedRect: ElementRect) => void;
    setSelectedRect: (selectedRect: ElementRect) => void;
    setRadius: (radius: number) => void;
    setShadowConfig: (shadowConfig: { shadowWidth: number; shadowColor: string }) => void;
    setSelectState: (selectState: SelectState) => void;
};

export const ResizeToolbar: React.FC<{
    actionRef: React.RefObject<ResizeToolbarActionType | undefined>;
    onSelectedRectChange: (selectedRect: ElementRect) => void;
    onRadiusChange: (radius: number) => void;
    onShadowConfigChange: (shadowConfig: { shadowWidth: number; shadowColor: string }) => void;
    getCaptureBoundingBoxInfo: () => CaptureBoundingBoxInfo | undefined;
}> = ({
    actionRef,
    onSelectedRectChange,
    onRadiusChange,
    onShadowConfigChange,
    getCaptureBoundingBoxInfo,
}) => {
    const { token } = theme.useToken();
    const intl = useIntl();

    const resizeToolbarRef = useRef<HTMLDivElement>(null);

    const [selectedRect, setSelectedRect, selectedRectRef] = useStateRef<ElementRect>({
        min_x: 0,
        min_y: 0,
        max_x: 0,
        max_y: 0,
    });
    const [radius, setRadius, radiusRef] = useStateRef(0);
    const [shadowConfig, setShadowConfig, shadowConfigRef] = useStateRef({
        shadowWidth: 0,
        shadowColor: '#00000000',
    });
    const [selectState, setSelectState] = useState(SelectState.Auto);

    const {
        contentScale: [, , contentScaleRef],
    } = useMonitorRect();

    const updateStyle = useCallback(
        (selectedRect: ElementRect) => {
            const resizeToolbar = resizeToolbarRef.current;
            if (!resizeToolbar) {
                return;
            }

            const { isBeyond } = updateElementPosition(
                resizeToolbar,
                0,
                0,
                new MousePosition(
                    0,
                    (resizeToolbar.clientHeight + token.marginXXS) * contentScaleRef.current,
                ),
                new MousePosition(
                    selectedRect.min_x / window.devicePixelRatio,
                    selectedRect.min_y / window.devicePixelRatio,
                ),
                undefined,
                true,
                contentScaleRef.current,
            );
            if (isBeyond) {
                updateElementPosition(
                    resizeToolbar,
                    0,
                    0,
                    new MousePosition(
                        -(selectedRect.max_x - selectedRect.min_x) / window.devicePixelRatio -
                            token.marginXXS * contentScaleRef.current,
                        0,
                    ),
                    new MousePosition(
                        selectedRect.min_x / window.devicePixelRatio,
                        selectedRect.min_y / window.devicePixelRatio,
                    ),
                    undefined,
                    undefined,
                    contentScaleRef.current,
                );
            }
        },
        [contentScaleRef, token.marginXXS],
    );

    const setEnable = useCallback((enable: boolean) => {
        const resizeToolbar = resizeToolbarRef.current;
        if (!resizeToolbar) {
            return;
        }

        if (enable) {
            resizeToolbar.style.opacity = '1';
        } else {
            resizeToolbar.style.opacity = '0';
        }
    }, []);

    const [getScreenshotType] = useStateSubscriber(ScreenshotTypePublisher, undefined);
    const [getCaptureEvent] = useStateSubscriber(CaptureEventPublisher, undefined);
    const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const updateEnable = useCallback(() => {
        const event = getCaptureEvent();
        if (!event) {
            return;
        }

        if (getScreenshotType() === ScreenshotType.TopWindow) {
            setEnable(false);
            return;
        }

        let isEnable = false;
        if (event.event === CaptureEvent.onCaptureReady) {
            updateStyle({ min_x: 0, min_y: 0, max_x: 0, max_y: 0 });
            isEnable = true;
        } else if (event.event === CaptureEvent.onCaptureFinish) {
            isEnable = false;
        } else if (event.event === CaptureEvent.onCaptureLoad) {
            isEnable = getDrawState() === DrawState.Idle;
        }

        setEnable(isEnable);
    }, [getCaptureEvent, getDrawState, getScreenshotType, setEnable, updateStyle]);
    const updateEnableDebounce = useMemo(() => {
        return debounce(updateEnable, 0);
    }, [updateEnable]);
    useStateSubscriber(CaptureEventPublisher, updateEnableDebounce);
    useStateSubscriber(DrawStatePublisher, updateEnableDebounce);

    useImperativeHandle(actionRef, () => {
        return {
            updateStyle,
            setSelectedRect: (selectedRect: ElementRect) => {
                setSelectedRect(selectedRect);
            },
            setRadius: (radius: number) => {
                setRadius(radius);
            },
            setShadowConfig: (shadowConfig: { shadowWidth: number; shadowColor: string }) => {
                setShadowConfig(shadowConfig);
            },
            setSelectState: (selectState: SelectState) => {
                setSelectState(selectState);
            },
        };
    }, [setRadius, setSelectedRect, setShadowConfig, updateStyle]);

    const updateSelectedRectCore = useCallback(
        (delta: ElementRect) => {
            const newSelectedRect = {
                min_x: selectedRectRef.current.min_x + delta.min_x,
                min_y: selectedRectRef.current.min_y + delta.min_y,
                max_x: selectedRectRef.current.max_x + delta.max_x,
                max_y: selectedRectRef.current.max_y + delta.max_y,
            };
            onSelectedRectChange(newSelectedRect);
        },
        [onSelectedRectChange, selectedRectRef],
    );
    const updateSelectedRect = useCallbackRender(updateSelectedRectCore);

    const changeMinX = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;

            updateSelectedRect({
                min_x: deltaValue,
                min_y: 0,
                max_x: deltaValue,
                max_y: 0,
            });
        },
        [updateSelectedRect],
    );
    const changeMinY = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;
            updateSelectedRect({
                min_x: 0,
                min_y: deltaValue,
                max_x: 0,
                max_y: deltaValue,
            });
        },
        [updateSelectedRect],
    );
    const changeWidth = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;
            updateSelectedRect({
                min_x: 0,
                min_y: 0,
                max_x: deltaValue,
                max_y: 0,
            });
        },
        [updateSelectedRect],
    );
    const changeHeight = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;
            updateSelectedRect({
                min_x: 0,
                min_y: 0,
                max_x: 0,
                max_y: deltaValue,
            });
        },
        [updateSelectedRect],
    );
    const changeRadiusCore = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;
            onRadiusChange(Math.min(Math.max(0, radiusRef.current + deltaValue), 256));
        },
        [onRadiusChange, radiusRef],
    );
    const changeRadius = useCallbackRender(changeRadiusCore);

    const changeShadowWidthCore = useCallback(
        (event: React.WheelEvent<HTMLDivElement>) => {
            const deltaValue = event.deltaY > 0 ? -1 : 1;
            onShadowConfigChange({
                shadowWidth: Math.min(
                    Math.max(0, shadowConfigRef.current.shadowWidth + deltaValue),
                    32,
                ),
                shadowColor: shadowConfigRef.current.shadowColor,
            });
        },
        [onShadowConfigChange, shadowConfigRef],
    );
    const changeShadowWidth = useCallbackRender(changeShadowWidthCore);

    const resizeModalActionRef = useRef<ResizeModalActionType | undefined>(undefined);
    const showResizeModal = useCallback(() => {
        const captureBoundingBoxInfo = getCaptureBoundingBoxInfo();
        if (!captureBoundingBoxInfo) {
            return;
        }

        resizeModalActionRef.current?.show(
            selectedRect,
            radius,
            shadowConfig,
            captureBoundingBoxInfo,
        );
    }, [getCaptureBoundingBoxInfo, selectedRect, radius, shadowConfig]);

    const onFinish = useCallback(
        async (params: ResizeModalParams) => {
            if (typeof params.shadowColor === 'object') {
                params.shadowColor = (params.shadowColor as AggregationColor).toHexString();
            }

            onSelectedRectChange({
                min_x: params.minX,
                min_y: params.minY,
                max_x: params.minX + params.width,
                max_y: params.minY + params.height,
            });
            onRadiusChange(params.radius);
            onShadowConfigChange({
                shadowWidth: params.shadowWidth,
                shadowColor: params.shadowColor as string,
            });

            return true;
        },
        [onRadiusChange, onSelectedRectChange, onShadowConfigChange],
    );
    return (
        <div className="draw-resize-toolbar" ref={resizeToolbarRef} onClick={showResizeModal}>
            <ResizeModal actionRef={resizeModalActionRef} onFinish={onFinish} />

            <Flex align="center" style={{ userSelect: 'none' }}>
                {selectState !== SelectState.Auto && (
                    <>
                        <Flex align="center">
                            <div
                                className="draw-resize-toolbar-scroll-value"
                                style={{ cursor: 'col-resize' }}
                                onWheel={changeMinX}
                                title={intl.formatMessage({ id: 'draw.positionX' })}
                            >
                                {selectedRect.min_x}
                            </div>
                            <div className="draw-resize-toolbar-symbol">{`,`}</div>
                            <div
                                className="draw-resize-toolbar-scroll-value"
                                style={{ cursor: 'row-resize' }}
                                onWheel={changeMinY}
                                title={intl.formatMessage({ id: 'draw.positionY' })}
                            >
                                {selectedRect.min_y}
                            </div>
                            <div className="draw-resize-toolbar-unit">{`px`}</div>
                        </Flex>
                        <div className="draw-resize-toolbar-splitter" />
                    </>
                )}
                <Flex align="center">
                    <div
                        className="draw-resize-toolbar-scroll-value"
                        style={{ cursor: 'col-resize' }}
                        onWheel={changeWidth}
                        title={intl.formatMessage({ id: 'draw.width' })}
                    >
                        {selectedRect.max_x - selectedRect.min_x}
                    </div>
                    <div className="draw-resize-toolbar-symbol">{`x`}</div>
                    <div
                        className="draw-resize-toolbar-scroll-value"
                        style={{ cursor: 'row-resize' }}
                        onWheel={changeHeight}
                        title={intl.formatMessage({ id: 'draw.height' })}
                    >
                        {selectedRect.max_y - selectedRect.min_y}
                    </div>
                    <div className="draw-resize-toolbar-unit">{`px`}</div>
                </Flex>
                {selectState !== SelectState.Auto && (
                    <>
                        <div className="draw-resize-toolbar-splitter" />
                        <Space align="center">
                            <div>
                                <div
                                    className="draw-resize-toolbar-scroll-value"
                                    style={{ cursor: 'row-resize' }}
                                    onWheel={changeRadius}
                                    title={intl.formatMessage({ id: 'draw.radius' })}
                                >
                                    <RadiusIcon
                                        style={{
                                            fontSize: '1.28em',
                                            marginRight: token.marginXXS,
                                            position: 'relative',
                                            top: '0.08em',
                                        }}
                                    />
                                    {radius}
                                </div>
                                <div className="draw-resize-toolbar-unit">{`px`}</div>
                            </div>

                            <div>
                                <div
                                    className="draw-resize-toolbar-scroll-value"
                                    style={{ cursor: 'row-resize' }}
                                    onWheel={changeShadowWidth}
                                    title={intl.formatMessage({ id: 'draw.shadowWidth' })}
                                >
                                    <ShadowIcon
                                        style={{
                                            fontSize: '1.28em',
                                            marginRight: token.marginXXS,
                                            position: 'relative',
                                            top: '0.08em',
                                        }}
                                    />
                                    {shadowConfig.shadowWidth}
                                </div>
                                <div className="draw-resize-toolbar-unit">{`px`}</div>
                            </div>
                        </Space>
                    </>
                )}
            </Flex>
            <style jsx>{`
                .draw-resize-toolbar {
                    box-shadow: 0 0 1px 0px ${token.colorPrimaryHover};
                    border-radius: ${token.borderRadius}px;
                    position: absolute;
                    top: 0;
                    left: 0;
                    padding: ${2}px ${token.paddingXS}px;
                    background-color: ${token.colorBgMask};
                    z-index: ${zIndexs.Draw_ResizeToolbar};
                    color: ${token.colorWhite};
                    cursor: pointer;
                    transition: box-shadow ${token.motionDurationFast} ${token.motionEaseInOut};
                    pointer-events: ${selectState === SelectState.Selected ||
                    selectState === SelectState.ScrollResize
                        ? 'auto'
                        : 'none'};
                    transform-origin: top left;
                }

                .draw-resize-toolbar:hover {
                    box-shadow: 0 0 6px 0px ${token.colorPrimaryHover};
                }

                .draw-resize-toolbar-splitter {
                    width: 1px;
                    height: 0.8em;
                    background-color: ${token.colorBorder};
                    opacity: 0.42;
                    margin: 0 ${token.marginXS + token.marginXXS}px;
                }

                .draw-resize-toolbar-scroll-value {
                    display: inline-block;
                    position: relative;
                }

                .draw-resize-toolbar-scroll-value::before {
                    content: '';
                    position: absolute;
                    top: 0;
                    left: -2px;
                    width: calc(100% + 4px);
                    height: calc(100% - 2px);
                    margin-top: 1px;
                    border-radius: ${token.borderRadius}px;
                    background-color: ${token.colorPrimary};
                    opacity: 0;
                    transition: opacity ${token.motionDurationFast} ${token.motionEaseInOut};
                }

                .draw-resize-toolbar-scroll-value:hover::before {
                    opacity: 0.42;
                }

                .draw-resize-toolbar-symbol {
                    display: inline-block;
                    position: relative;
                    margin: 0 ${token.marginXXS}px;
                }

                .draw-resize-toolbar-unit {
                    display: inline-block;
                    position: relative;
                    margin-left: ${token.marginXXS}px;
                }
            `}</style>
        </div>
    );
};
