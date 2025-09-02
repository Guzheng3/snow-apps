'use client';

import { useCallback, useMemo } from 'react';

import { DrawCore } from '@/app/fullScreenDraw/components/drawCore';
import { DrawCacheLayerActionType } from './extra';
import { useRef, useImperativeHandle, useContext } from 'react';
import {
    DrawCoreActionType,
    DrawCoreContext,
    DrawCoreContextValue,
    ExcalidrawEventPublisher,
} from '@/app/fullScreenDraw/components/drawCore/extra';
import React from 'react';
import { useHistory } from '@/app/fullScreenDraw/components/drawCore/components/historyContext';
import { zIndexs } from '@/utils/zIndex';
import { DrawContext } from '../../types';
import { ElementRect } from '@/commands';
import { theme } from 'antd';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { ExcalidrawPropsCustomOptions, NormalizedZoomValue } from '@mg-chao/excalidraw/types';

const DrawCacheLayerCore: React.FC<{
    actionRef: React.RefObject<DrawCacheLayerActionType | undefined>;
}> = ({ actionRef }) => {
    const { token } = theme.useToken();
    const drawCoreActionRef = useRef<DrawCoreActionType | undefined>(undefined);
    const { mousePositionRef } = useContext(DrawContext);
    const [, setExcalidrawEvent] = useStateSubscriber(ExcalidrawEventPublisher, undefined);

    const { history } = useHistory();

    /**
     * 结束绘制，终止画布正在进行的绘制操作
     */
    const finishDraw = useCallback(() => {
        drawCoreActionRef.current?.finishDraw();
    }, []);

    const clearHistory = useCallback(() => {
        drawCoreActionRef.current?.getExcalidrawAPI()?.history.clear();
        history.clear();
    }, [history]);

    useImperativeHandle(
        actionRef,
        () => ({
            setActiveTool: (...args) => {
                drawCoreActionRef.current?.setActiveTool(...args);
            },
            syncActionResult: (...args) => {
                drawCoreActionRef.current?.syncActionResult(...args);
            },
            updateScene: (...args) => {
                drawCoreActionRef.current?.updateScene(...args);
            },
            onCaptureReady: async () => {
                drawCoreActionRef.current?.updateScene({
                    elements: [],
                });
                setExcalidrawEvent({
                    event: 'onDraw',
                    params: undefined,
                });
                setExcalidrawEvent(undefined);
            },
            finishDraw,
            onCaptureFinish: async () => {
                drawCoreActionRef.current?.setActiveTool({
                    type: 'hand',
                });
                drawCoreActionRef.current?.updateScene({
                    elements: [],
                    appState: {
                        // 清除在编辑中的元素
                        newElement: null,
                        editingTextElement: null,
                        selectedLinearElement: null,
                        zoom: {
                            value: 1 as NormalizedZoomValue,
                        },
                        scrollX: 0,
                        scrollY: 0,
                    },
                    captureUpdate: 'IMMEDIATELY',
                });
                clearHistory();
            },
            getAppState: () => {
                return drawCoreActionRef.current?.getAppState();
            },
            getImageData: async (...args) => {
                return await drawCoreActionRef.current?.getImageData(...args);
            },
            getCanvasContext: () => {
                return drawCoreActionRef.current?.getCanvasContext();
            },
            getCanvas: () => {
                return drawCoreActionRef.current?.getCanvas();
            },
            getDrawCacheLayerElement: () => {
                return drawCoreActionRef.current?.getDrawCacheLayerElement();
            },
            getExcalidrawAPI: () => {
                return drawCoreActionRef.current?.getExcalidrawAPI();
            },
            clearHistory,
        }),
        [clearHistory, finishDraw, setExcalidrawEvent],
    );

    const { selectLayerActionRef } = useContext(DrawContext);
    const drawCoreContextValue = useMemo<DrawCoreContextValue>(() => {
        return {
            getLimitRect: () => {
                return selectLayerActionRef.current?.getSelectRect();
            },
            getDevicePixelRatio: () => {
                return window.devicePixelRatio;
            },
            getBaseOffset: (limitRect: ElementRect, devicePixelRatio: number) => {
                return {
                    x: limitRect.max_x / devicePixelRatio + token.marginXXS,
                    y: limitRect.min_y / devicePixelRatio,
                };
            },
            getAction: () => {
                return drawCoreActionRef.current;
            },
            getMousePosition: () => {
                return mousePositionRef.current;
            },
        };
    }, [selectLayerActionRef, token.marginXXS, mousePositionRef]);

    const excalidrawCustomOptions = useMemo<NonNullable<ExcalidrawPropsCustomOptions>>(() => {
        return {
            getReferenceSnapPoints: (defaultFn) => {
                return (...params: Parameters<typeof defaultFn>) => {
                    const appState = params[2];
                    const selectRect = selectLayerActionRef.current?.getSelectRect();

                    const innerPadding = 3 * window.devicePixelRatio;

                    const selectRectPoints: [number, number][] = selectRect
                        ? [
                              [selectRect.min_x + innerPadding, selectRect.min_y + innerPadding],
                              [selectRect.max_x - innerPadding, selectRect.min_y + innerPadding],
                              [selectRect.max_x - innerPadding, selectRect.max_y - innerPadding],
                              [selectRect.min_x + innerPadding, selectRect.max_y - innerPadding],
                          ].map(([x, y]) => {
                              const canvasX =
                                  x / appState.zoom.value / window.devicePixelRatio -
                                  appState.scrollX;
                              const canvasY =
                                  y / appState.zoom.value / window.devicePixelRatio -
                                  appState.scrollY;

                              return [canvasX, canvasY];
                          })
                        : [];

                    return defaultFn(...params).concat(
                        selectRectPoints as ReturnType<typeof defaultFn>,
                    );
                };
            },
        };
    }, [selectLayerActionRef]);

    return (
        <DrawCoreContext.Provider value={drawCoreContextValue}>
            <DrawCore
                actionRef={drawCoreActionRef}
                zIndex={zIndexs.Draw_DrawCacheLayer}
                layoutMenuZIndex={zIndexs.Draw_ExcalidrawToolbar}
                excalidrawCustomOptions={excalidrawCustomOptions}
                appStateStorageKey={'draw-cache-layer'}
            />
        </DrawCoreContext.Provider>
    );
};

export const DrawCacheLayer = React.memo(DrawCacheLayerCore);
