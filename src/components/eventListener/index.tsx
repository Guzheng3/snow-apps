'use client';

import { EventCallback, UnlistenFn } from '@tauri-apps/api/event';
import { createContext, useCallback, useContext, useEffect, useMemo, useRef } from 'react';
import { attachConsole } from '@tauri-apps/plugin-log';
import { appLog, LogMessageEvent } from '@/utils/appLog';
import React from 'react';
import { MenuLayoutContext } from '@/app/menuLayout';
import { getCurrentWindow, Window as AppWindow } from '@tauri-apps/api/window';
import { AppSettingsActionContext } from '@/app/contextWrap';
import {
    LISTEN_KEY_SERVICE_KEY_DOWN_EMIT_KEY,
    LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY,
    LISTEN_KEY_SERVICE_STOP_EMIT_KEY,
    ListenKeyCode,
    ListenKeyDownEvent,
    listenKeyStop,
    listenKeyStopByWindowLabel,
    ListenKeyUpEvent,
} from '@/commands/listenKey';
import { showWindow } from '@/utils/window';
import { AntdContext } from '../globalLayoutExtra';
import { appError } from '@/utils/log';
import { debounce } from 'es-toolkit';
import { ocrRelease } from '@/commands/ocr';
import {
    FIXED_CONTENT_FOCUS_MODE_CLOSE_ALL_WINDOW,
    FIXED_CONTENT_FOCUS_MODE_CLOSE_OTHER_WINDOW,
    FIXED_CONTENT_FOCUS_MODE_HIDE_OTHER_WINDOW,
    FIXED_CONTENT_FOCUS_MODE_SHOW_ALL_WINDOW,
} from '@/functions/fixedContent';

type Listener = {
    event: string;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    callback: EventCallback<any>;
};

export type EventListenerContextType = {
    addListener: (event: string, listener: (payload: unknown) => void) => number;
    removeListener: (id: number) => boolean;
};

export const EventListenerContext = createContext<EventListenerContextType>({
    addListener: () => 0,
    removeListener: () => false,
});
/**
 * 监听 tauri 的消息
 */
const EventListenerCore: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const { message } = useContext(AntdContext);

    const { pathname, mainWindow } = useContext(MenuLayoutContext);
    const appWindowRef = useRef<AppWindow | undefined>(undefined);
    useEffect(() => {
        appWindowRef.current = getCurrentWindow();
    }, []);

    const listenerCount = useRef<number>(0);
    const listenerMapRef = useRef<Map<number, Listener>>(new Map());
    const listenerEventMapRef = useRef<Map<string, Set<number>>>(new Map());
    const addListener = useCallback((event: string, listener: (payload: unknown) => void) => {
        listenerCount.current++;
        const listenerId = listenerCount.current;
        listenerMapRef.current.set(listenerId, { event, callback: listener });

        let listenerList = listenerEventMapRef.current.get(event);
        if (!listenerList) {
            listenerList = new Set();
        }
        listenerList.add(listenerId);
        listenerEventMapRef.current.set(event, listenerList);

        return listenerId;
    }, []);
    const removeListener = useCallback((listenerId: number): boolean => {
        const listener = listenerMapRef.current.get(listenerId);
        if (!listener) {
            return false;
        }

        let res = true;
        res = (res && listenerEventMapRef.current.get(listener.event)?.delete(listenerId)) ?? false;
        res = res && listenerMapRef.current.delete(listenerId);

        return res;
    }, []);

    const { reloadAppSettings } = useContext(AppSettingsActionContext);

    const inited = useRef(false);
    const {
        isDrawPage,
        isFullScreenDraw,
        isFullScreenDrawSwitchMouseThrough,
        isVideoRecordPage,
        isVideoRecordToolbarPage,
    } = useMemo(() => {
        let isDrawPage = false;
        let isFullScreenDraw = false;
        let isFullScreenDrawSwitchMouseThrough = false;
        let isVideoRecordPage = false;
        let isVideoRecordToolbarPage = false;
        if (pathname === '/draw') {
            isDrawPage = true;
        } else if (pathname === '/fullScreenDraw') {
            isFullScreenDraw = true;
        } else if (pathname === '/fullScreenDraw/switchMouseThrough') {
            isFullScreenDrawSwitchMouseThrough = true;
        } else if (pathname === '/videoRecord') {
            isVideoRecordPage = true;
        } else if (pathname === '/videoRecord/toolbar') {
            isVideoRecordToolbarPage = true;
        }
        return {
            isDrawPage,
            isFullScreenDraw,
            isFullScreenDrawSwitchMouseThrough,
            isVideoRecordPage,
            isVideoRecordToolbarPage,
        };
    }, [pathname]);

    const releaseOcrSessionAction = useMemo(() => {
        return debounce(async () => {
            await ocrRelease();
        }, 16 * 1000);
    }, []);
    useEffect(() => {
        if (inited.current) {
            return;
        }
        inited.current = true;

        let detach: UnlistenFn;
        attachConsole().then((d) => {
            detach = d;
        });

        const unlistenList: UnlistenFn[] = [];
        const defaultListener: Listener[] = [];

        const listenKeyCallback = (
            emitKey:
                | typeof LISTEN_KEY_SERVICE_KEY_DOWN_EMIT_KEY
                | typeof LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY,
        ) => {
            let type = 'keydown';
            if (emitKey === LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY) {
                type = 'keyup';
            }

            return async ({ payload }: { payload: ListenKeyDownEvent | ListenKeyUpEvent }) => {
                switch (payload.key) {
                    case ListenKeyCode.RControl:
                    case ListenKeyCode.LControl:
                        document.dispatchEvent(
                            new KeyboardEvent(type, {
                                code: 'ControlLeft',
                                key: 'Control',
                                keyCode: 17,
                                which: 17,
                            }),
                        );
                        break;
                    case ListenKeyCode.RShift:
                    case ListenKeyCode.LShift:
                        document.dispatchEvent(
                            new KeyboardEvent(type, {
                                code: 'ShiftLeft',
                                key: 'Shift',
                                keyCode: 16,
                                which: 16,
                            }),
                        );
                        break;
                    case ListenKeyCode.Command:
                    case ListenKeyCode.RCommand:
                        document.dispatchEvent(
                            new KeyboardEvent(type, {
                                code: 'MetaLeft',
                                key: 'Meta',
                                keyCode: 224,
                                which: 224,
                            }),
                        );
                        break;
                    case ListenKeyCode.LOption:
                    case ListenKeyCode.ROption:
                        document.dispatchEvent(
                            new KeyboardEvent(type, {
                                code: 'AltLeft',
                                key: 'Alt',
                                keyCode: 18,
                                which: 18,
                            }),
                        );
                        break;
                    default:
                        break;
                }
            };
        };

        defaultListener.push({
            event: LISTEN_KEY_SERVICE_KEY_DOWN_EMIT_KEY,
            callback: listenKeyCallback(LISTEN_KEY_SERVICE_KEY_DOWN_EMIT_KEY),
        });
        defaultListener.push({
            event: LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY,
            callback: listenKeyCallback(LISTEN_KEY_SERVICE_KEY_UP_EMIT_KEY),
        });

        if (mainWindow) {
            defaultListener.push({
                event: 'release-ocr-session',
                callback: async () => {
                    releaseOcrSessionAction();
                },
            });
            defaultListener.push({
                event: 'log-message',
                callback: ({ payload }: { payload: LogMessageEvent }) => {
                    appLog(payload, undefined, 'APP_TAURI');
                },
            });
            defaultListener.push({
                event: 'execute-chat',
                callback: async () => {},
            });
            defaultListener.push({
                event: 'execute-chat-selected-text',
                callback: async () => {},
            });
            defaultListener.push({
                event: 'execute-translate',
                callback: async () => {},
            });
            defaultListener.push({
                event: 'execute-translate-selected-text',
                callback: async () => {},
            });
            defaultListener.push({
                event: 'main-window:send-error-message',
                callback: async ({ payload }: { payload: string }) => {
                    showWindow();
                    message.error(payload);
                },
            });
            defaultListener.push({
                event: LISTEN_KEY_SERVICE_STOP_EMIT_KEY,
                callback: async ({ payload }: { payload: string }) => {
                    listenKeyStopByWindowLabel(payload);
                },
            });
            defaultListener.push({
                event: 'on-hide-main-window',
                callback: async () => {},
            });
        } else {
            defaultListener.push({
                event: FIXED_CONTENT_FOCUS_MODE_SHOW_ALL_WINDOW,
                callback: async () => {},
            });
            defaultListener.push({
                event: FIXED_CONTENT_FOCUS_MODE_HIDE_OTHER_WINDOW,
                callback: async () => {},
            });
            defaultListener.push({
                event: FIXED_CONTENT_FOCUS_MODE_CLOSE_OTHER_WINDOW,
                callback: async () => {},
            });
            defaultListener.push({
                event: FIXED_CONTENT_FOCUS_MODE_CLOSE_ALL_WINDOW,
                callback: async () => {},
            });

            defaultListener.push({
                event: 'reload-app-settings',
                callback: async () => {
                    reloadAppSettings();
                },
            });

            if (isDrawPage) {
                defaultListener.push({
                    event: 'execute-screenshot',
                    callback: async () => {},
                });
                defaultListener.push({
                    event: 'finish-screenshot',
                    callback: async () => {},
                });
                defaultListener.push({
                    event: 'release-draw-page',
                    callback: async () => {},
                });
            }

            if (isFullScreenDraw || isFullScreenDrawSwitchMouseThrough) {
                defaultListener.push({
                    event: 'full-screen-draw-change-mouse-through',
                    callback: async () => {},
                });
            }

            if (isFullScreenDrawSwitchMouseThrough) {
                defaultListener.push({
                    event: 'close-full-screen-draw',
                    callback: async () => {},
                });
            }

            if (isVideoRecordPage || isVideoRecordToolbarPage) {
                defaultListener.push({
                    event: 'reload-video-record',
                    callback: async () => {},
                });
            }

            if (isVideoRecordPage) {
                defaultListener.push({
                    event: 'close-video-record-window',
                    callback: async () => {},
                });
                defaultListener.push({
                    event: 'change-video-record-state',
                    callback: async () => {},
                });
            }
        }

        defaultListener
            .map((listener) => {
                const res: Listener = {
                    event: listener.event,
                    callback: (e) => {
                        listener.callback(e);
                        try {
                            listenerEventMapRef.current.get(listener.event)?.forEach((id) => {
                                listenerMapRef.current.get(id)?.callback(e);
                            });
                        } catch (error) {
                            appError('[EventListenerCore] callback error', error);
                        }
                    },
                };
                return res;
            })
            .forEach((listener) => {
                appWindowRef.current!.listen(listener.event, listener.callback).then((unlisten) => {
                    unlistenList.push(unlisten);
                });
            });

        const clear = () => {
            listenKeyStop();

            detach?.();

            unlistenList.forEach((unlisten) => {
                try {
                    unlisten();
                } catch (error) {
                    appError('[EventListenerCore] clear unlisten error', error);
                }
            });

            inited.current = false;
        };

        window.addEventListener('beforeunload', () => {
            clear();
        });

        return () => {
            clear();
            window.removeEventListener('beforeunload', clear);
        };
    }, [
        mainWindow,
        isDrawPage,
        reloadAppSettings,
        isFullScreenDraw,
        isFullScreenDrawSwitchMouseThrough,
        isVideoRecordPage,
        isVideoRecordToolbarPage,
        message,
        releaseOcrSessionAction,
    ]);

    const eventListenerContextValue = useMemo(() => {
        return { addListener, removeListener };
    }, [addListener, removeListener]);
    return (
        <EventListenerContext.Provider value={eventListenerContextValue}>
            {children}
        </EventListenerContext.Provider>
    );
};

export const EventListener = React.memo(EventListenerCore);
