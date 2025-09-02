'use client';

import { useContext, useEffect, useMemo, useRef, useState } from 'react';
import {
    DrawCoreActionType,
    DrawCoreContext,
    DrawCoreContextValue,
    DrawState,
    DrawStatePublisher,
    ExcalidrawEventPublisher,
} from './components/drawCore/extra';
import { zIndexs } from '@/utils/zIndex';
import dynamic from 'next/dynamic';
import { debounce } from 'es-toolkit';
import { ElementRect } from '@/commands';
import { theme } from 'antd';
import { DrawToolbarActionType, FullScreenDrawToolbar } from './components/toolbar';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { MousePosition } from '@/utils/mousePosition';
import { EnableKeyEventPublisher } from '../draw/components/drawToolbar/components/keyEventWrap/extra';
import { EventListenerContext } from '@/components/eventListener';
import { DrawContext, DrawContextType } from './extra';

const DrawCore = dynamic(
    async () => (await import('../fullScreenDraw/components/drawCore')).DrawCore,
    {
        ssr: false,
    },
);

const FullScreenDrawPage = () => {
    const { token } = theme.useToken();
    const drawCoreActionRef = useRef<DrawCoreActionType | undefined>(undefined);
    const toolbarActionRef = useRef<DrawToolbarActionType | undefined>(undefined);

    const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const [, setExcalidrawEvent] = useStateSubscriber(ExcalidrawEventPublisher, undefined);
    const [, setEnableKeyEvent] = useStateSubscriber(EnableKeyEventPublisher, undefined);

    const { addListener, removeListener } = useContext(EventListenerContext);

    const limitRectRef = useRef<ElementRect | undefined>(undefined);

    const fullScreenContainerRef = useRef<HTMLDivElement | null>(null);

    const inited = useRef(false);
    const init = useMemo(() => {
        return debounce(() => {
            limitRectRef.current = {
                min_x: 0,
                min_y: 0,
                max_x: window.screen.width * window.devicePixelRatio,
                max_y: window.screen.height * window.devicePixelRatio,
            };

            if (inited.current) {
                return;
            }

            inited.current = true;

            setExcalidrawEvent({
                event: 'onDraw',
                params: undefined,
            });
            setExcalidrawEvent(undefined);
            setEnableKeyEvent(true);

            getCurrentWindow().setFocus();

            if (process.env.NODE_ENV === 'development') {
                getCurrentWindow().setAlwaysOnTop(false);
            }
        }, 0);
    }, [setEnableKeyEvent, setExcalidrawEvent]);

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

    const drawCoreContextValue = useMemo<DrawCoreContextValue>(() => {
        return {
            getLimitRect: () => {
                return limitRectRef.current;
            },
            getDevicePixelRatio: () => {
                return window.devicePixelRatio;
            },
            getBaseOffset: (limitRect: ElementRect, devicePixelRatio: number) => {
                return {
                    x: limitRect.min_x / devicePixelRatio + token.margin,
                    y: (limitRect.max_y - limitRect.min_x) / 10 / devicePixelRatio + token.margin,
                };
            },
            getAction: () => {
                return drawCoreActionRef.current;
            },
            getMousePosition: () => {
                return mousePositionRef.current;
            },
        };
    }, [token.margin]);

    const drawContextValue = useMemo<DrawContextType>(() => {
        return {
            getDrawCoreAction: () => drawCoreActionRef.current,
        };
    }, []);

    const [enableMouseThrough, setEnableMouseThrough] = useState(false);
    const mosueThroughStateRef = useRef({
        enable: false,
        tool: DrawState.Select,
    });
    useEffect(() => {
        const listenerId = addListener('full-screen-draw-change-mouse-through', () => {
            if (mosueThroughStateRef.current.enable) {
                mosueThroughStateRef.current.enable = false;
                getCurrentWindow().setIgnoreCursorEvents(false);

                toolbarActionRef.current?.setTool(mosueThroughStateRef.current.tool);
            } else {
                mosueThroughStateRef.current.enable = true;
                getCurrentWindow().setIgnoreCursorEvents(true);

                mosueThroughStateRef.current.tool = getDrawState();
                toolbarActionRef.current?.setTool(DrawState.MouseThrough);
            }

            setEnableMouseThrough(mosueThroughStateRef.current.enable);
        });

        return () => {
            removeListener(listenerId);
        };
    }, [addListener, removeListener, getDrawState]);
    return (
        <DrawContext.Provider value={drawContextValue}>
            <DrawCoreContext.Provider value={drawCoreContextValue}>
                <div
                    className="full-screen-draw-page"
                    ref={fullScreenContainerRef}
                    onContextMenu={(e) => {
                        e.preventDefault();
                    }}
                >
                    <DrawCore
                        actionRef={drawCoreActionRef}
                        zIndex={zIndexs.FullScreenDraw_DrawLayer}
                        layoutMenuZIndex={zIndexs.FullScreenDraw_LayoutMenu}
                        onLoad={init}
                        appStateStorageKey={'full-screen-draw-page'}
                    />

                    <FullScreenDrawToolbar actionRef={toolbarActionRef} />

                    <style jsx>{`
                        .full-screen-draw-page {
                            position: fixed;
                            top: 0;
                            left: 0;
                            width: 100vw;
                            height: 100vh;
                        }

                        .full-screen-draw-page :global(.full-screen-draw-toolbar-container) {
                            opacity: ${enableMouseThrough ? '0' : '1'};
                            transition: opacity ${token.motionDurationSlow} ${token.motionEaseInOut};
                        }
                    `}</style>
                </div>
            </DrawCoreContext.Provider>
        </DrawContext.Provider>
    );
};

export default withStatePublisher(
    FullScreenDrawPage,
    DrawStatePublisher,
    ExcalidrawEventPublisher,
    EnableKeyEventPublisher,
);
