import React, {
    useCallback,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
    useState,
} from 'react';
import dynamic from 'next/dynamic';
import {
    DrawCoreActionType,
    DrawCoreContext,
    DrawCoreContextValue,
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventPublisher,
} from '@/app/fullScreenDraw/components/drawCore/extra';
import { zIndexs } from '@/utils/zIndex';
import { FixedContentWindowSize } from '../..';
import { theme } from 'antd';
import { ElementRect } from '@/commands';
import { MousePosition } from '@/utils/mousePosition';
import { FixedContentCoreDrawToolbar, FixedContentCoreDrawToolbarActionType } from './toolbar';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { EnableKeyEventPublisher } from '@/app/draw/components/drawToolbar/components/keyEventWrap/extra';
import { DrawContext, DrawContextType } from '@/app/fullScreenDraw/extra';
import { withCanvasHistory } from '@/app/fullScreenDraw/components/drawCore/components/historyContext';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { NormalizedZoomValue } from '@mg-chao/excalidraw/types';

const DrawCore = dynamic(
    async () => (await import('../../../../../fullScreenDraw/components/drawCore')).DrawCore,
    {
        ssr: false,
    },
);

export type FixedContentCoreDrawActionType = {
    getToolbarSize: () => { width: number; height: number };
    getDrawMenuSize: () => { width: number; height: number };
    getCanvas: () => HTMLCanvasElement | null;
};

const DRAW_MENU_WIDTH = 200;
const DRAW_MENU_HEIGHT = 300;

const DrawLayerCore: React.FC<{
    actionRef: React.RefObject<FixedContentCoreDrawActionType | undefined>;
    documentSize: FixedContentWindowSize;
    scaleInfo: {
        x: number;
        y: number;
    };
    disabled?: boolean;
    hidden?: boolean;
    onConfirm: () => void;
}> = ({ actionRef, documentSize, scaleInfo, disabled, hidden, onConfirm }) => {
    const { token } = theme.useToken();

    const drawToolbarActionRef = useRef<FixedContentCoreDrawToolbarActionType | undefined>(
        undefined,
    );
    const drawCoreActionRef = useRef<DrawCoreActionType | undefined>(undefined);
    const [, setEnableKeyEvent] = useStateSubscriber(EnableKeyEventPublisher, undefined);

    const [excalidrawReady, setExcalidrawReady] = useState(false);
    useEffect(() => {
        if (!disabled) {
            setExcalidrawReady(true);
        }
    }, [disabled]);

    const mousePositionRef = useRef<MousePosition | undefined>(undefined);
    useEffect(() => {
        const onMouseMove = (ev: MouseEvent) => {
            mousePositionRef.current = new MousePosition(ev.clientX, ev.clientY);
        };

        document.addEventListener('mousemove', onMouseMove);

        return () => {
            document.removeEventListener('mousemove', onMouseMove);
        };
    }, []);

    const getDrawMenuSize = useCallback(() => {
        return {
            width: DRAW_MENU_WIDTH + 3 * 2,
            height: DRAW_MENU_HEIGHT + 3 * 2 + 36,
        };
    }, []);

    const drawCoreContextValue = useMemo<DrawCoreContextValue>(() => {
        return {
            getLimitRect: () => {
                return {
                    min_x: 0,
                    min_y: 0,
                    max_x: documentSize.width * window.devicePixelRatio,
                    max_y: documentSize.height * window.devicePixelRatio,
                };
            },
            getDevicePixelRatio: () => {
                return window.devicePixelRatio;
            },
            getBaseOffset: (limitRect: ElementRect, devicePixelRatio: number) => {
                return {
                    x: limitRect.max_x / devicePixelRatio + token.marginXXS,
                    y: limitRect.min_x / devicePixelRatio + 3,
                };
            },
            getAction: () => {
                return drawCoreActionRef.current;
            },
            getMousePosition: () => {
                return mousePositionRef.current;
            },
        };
    }, [documentSize.height, documentSize.width, token.marginXXS]);

    useImperativeHandle(actionRef, () => {
        return {
            getToolbarSize: () => {
                return (
                    drawToolbarActionRef.current?.getSize() ?? {
                        width: 0,
                        height: 0,
                    }
                );
            },
            getDrawMenuSize,
            getCanvas: () => {
                if (
                    drawCoreActionRef.current?.getExcalidrawAPI()?.getSceneElements().length === 0
                ) {
                    return null;
                }

                return drawCoreActionRef.current?.getCanvas() ?? null;
            },
        };
    }, [getDrawMenuSize]);

    const drawContextValue = useMemo<DrawContextType>(() => {
        return {
            getDrawCoreAction: () => drawCoreActionRef.current,
        };
    }, []);

    const onMouseEvent = useCallback((event: React.MouseEvent<HTMLDivElement>) => {
        event.preventDefault();
        event.stopPropagation();
    }, []);

    useEffect(() => {
        setEnableKeyEvent(!(disabled ?? false));
    }, [disabled, setEnableKeyEvent]);

    useEffect(() => {
        if (!drawCoreActionRef.current) {
            return;
        }

        const appState = drawCoreActionRef.current?.getAppState();
        if (!appState) {
            return;
        }

        drawCoreActionRef.current?.updateScene({
            appState: {
                scrollX: appState.scrollX,
                scrollY: appState.scrollY,
                zoom: {
                    value: (scaleInfo.x / 100) as NormalizedZoomValue,
                },
            },
            captureUpdate: 'NEVER',
        });
    }, [scaleInfo.x, drawCoreActionRef]);

    useEffect(() => {
        if (disabled) {
            drawCoreActionRef.current?.finishDraw();
            drawToolbarActionRef.current?.setTool(DrawState.Select);
        }
    }, [drawCoreActionRef, disabled]);

    return (
        <DrawContext.Provider value={drawContextValue}>
            <div
                className="fixed-content-draw-layer"
                onMouseDown={onMouseEvent}
                onClick={onMouseEvent}
                onDoubleClick={onMouseEvent}
            >
                <DrawCoreContext.Provider value={drawCoreContextValue}>
                    <DrawCore
                        actionRef={drawCoreActionRef}
                        zIndex={zIndexs.Draw_DrawCacheLayer}
                        layoutMenuZIndex={zIndexs.Draw_ExcalidrawToolbar}
                        appStateStorageKey={'fixed-content-draw-layer'}
                    />

                    <FixedContentCoreDrawToolbar
                        actionRef={drawToolbarActionRef}
                        disabled={disabled}
                        documentSize={documentSize}
                        onConfirm={() => {
                            onConfirm();
                        }}
                    />
                </DrawCoreContext.Provider>

                <style jsx>{`
                    .fixed-content-draw-layer {
                        position: absolute;
                        top: 0;
                        left: 0;
                        width: ${documentSize.width}px;
                        height: ${documentSize.height}px;
                        opacity: ${!excalidrawReady || hidden ? 0 : 1};
                        pointer-events: ${disabled ? 'none' : 'auto'};
                    }

                    .fixed-content-draw-layer :global(.Island.App-menu__left) {
                        max-height: ${Math.max(
                            documentSize.height + 19,
                            DRAW_MENU_HEIGHT + 15,
                        )}px !important;
                    }
                `}</style>
            </div>
        </DrawContext.Provider>
    );
};

export const DrawLayer = withCanvasHistory(
    withStatePublisher(
        DrawLayerCore,
        DrawStatePublisher,
        ExcalidrawEventPublisher,
        EnableKeyEventPublisher,
    ),
);
