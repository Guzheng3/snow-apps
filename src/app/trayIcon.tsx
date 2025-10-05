'use client';

import { defaultWindowIcon } from '@tauri-apps/api/app';
import { TrayIcon, TrayIconOptions } from '@tauri-apps/api/tray';
import { useCallback, useContext, useEffect, useState } from 'react';
import { getCurrentWindow } from '@tauri-apps/api/window';
import React from 'react';
import { Menu } from '@tauri-apps/api/menu';
import { useIntl } from 'react-intl';
import { exitApp } from '@/commands';
import { showWindow } from '@/utils/window';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import {
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
    TrayIconClickAction,
    TrayIconDefaultIcon,
} from './contextWrap';
import { isEqual } from 'es-toolkit';
import { AppFunction, AppFunctionConfig } from './extra';
import {
    executeScreenshot,
    executeScreenshotFocusedWindow,
    ScreenshotType,
} from '@/functions/screenshot';
import { useStateRef } from '@/hooks/useStateRef';
import {
    executeChat,
    executeChatSelectedText,
    executeTranslate,
    executeTranslateSelectedText,
} from '@/functions/tools';
import { createPublisher } from '@/hooks/useStatePublisher';
import { AntdContext } from '@/components/globalLayoutExtra';
import { Image } from '@tauri-apps/api/image';
import { createFixedContentWindow, createFullScreenDrawWindow } from '@/commands/core';
import { getPlatformValue } from '@/utils';
import { join, resourceDir } from '@tauri-apps/api/path';
import { convertFileSrc } from '@tauri-apps/api/core';
import { appError } from '@/utils/log';
import { formatKey } from '@/utils/format';
import { startOrCopyVideo } from '@/functions/videoRecord';

export const TrayIconStatePublisher = createPublisher<{
    disableShortcut: boolean;
}>({
    disableShortcut: false,
});

export const getDefaultIconPath = async (
    defaultIcon: TrayIconDefaultIcon,
    resourceDirPath?: string,
): Promise<{
    web_path: string;
    native_path: string;
}> => {
    const basePath = resourceDirPath ?? (await resourceDir());

    const nativePath = await join(basePath, 'app-icons', `snow-shot-tray-${defaultIcon}.png`);
    const defaultIconPath = convertFileSrc(nativePath);

    return {
        web_path: defaultIconPath,
        native_path: nativePath,
    };
};

const trayIconId = 'snow-shot-tray-icon';
let trayIcon: TrayIcon | undefined = undefined;
const TrayIconLoaderComponent = () => {
    const intl = useIntl();
    const { message } = useContext(AntdContext);
    const [disableShortcut, _setDisableShortcut] = useState(false);
    const [, setTrayIconState] = useStateSubscriber(
        TrayIconStatePublisher,
        useCallback(
            (state: { disableShortcut: boolean }) => {
                _setDisableShortcut(state.disableShortcut);
            },
            [_setDisableShortcut],
        ),
    );

    const [delayScreenshotSeconds, setDelayScreenshotSeconds] = useState(0);
    const [shortcutKeys, setShortcutKeys, shortcutKeysRef] = useStateRef<
        Record<AppFunction, AppFunctionConfig> | undefined
    >(undefined);
    const [iconPath, setIconPath] = useState('');
    const [defaultIcon, setDefaultIcon] = useState<TrayIconDefaultIcon>(
        TrayIconDefaultIcon.Default,
    );
    const [enableTrayIcon, setEnableTrayIcon] = useState(false);
    const [getAppSettings] = useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (settings: AppSettingsData, previous: AppSettingsData) => {
                if (
                    shortcutKeysRef.current === undefined ||
                    !isEqual(
                        settings[AppSettingsGroup.AppFunction],
                        previous[AppSettingsGroup.AppFunction],
                    )
                ) {
                    setShortcutKeys(settings[AppSettingsGroup.AppFunction]);
                }

                setIconPath(settings[AppSettingsGroup.CommonTrayIcon].iconPath);
                setDefaultIcon(settings[AppSettingsGroup.CommonTrayIcon].defaultIcons);
                setEnableTrayIcon(settings[AppSettingsGroup.CommonTrayIcon].enableTrayIcon);
                setDelayScreenshotSeconds(settings[AppSettingsGroup.Cache].delayScreenshotSeconds);
            },
            [setShortcutKeys, shortcutKeysRef, setIconPath, setDefaultIcon],
        ),
    );

    const closeTrayIcon = useCallback(async () => {
        try {
            if (trayIcon) {
                await trayIcon.close();
                trayIcon = undefined;
            }

            const trayIcon2 = await TrayIcon.getById(trayIconId);
            if (trayIcon2) {
                await trayIcon2.close();
            }
        } catch (error) {
            console.warn(`[closeTrayIcon] error`, error);
        }
    }, []);

    const initTrayIcon = useCallback(async () => {
        if (!shortcutKeys) {
            return;
        }

        if (!enableTrayIcon) {
            closeTrayIcon();
            return;
        }

        const appWindow = getCurrentWindow();

        let iconImage: Image | undefined = undefined;
        try {
            if (iconPath) {
                iconImage = await Image.fromPath(iconPath);
            }
        } catch {
            message.error(intl.formatMessage({ id: 'home.trayIcon.error4' }));
            return;
        }

        if (iconImage) {
            const size = await iconImage.size();
            if (size.width > 128 || size.height > 128) {
                message.error(intl.formatMessage({ id: 'home.trayIcon.error3' }));
                return;
            }
        }

        const options: TrayIconOptions = {
            id: trayIconId,
            icon: iconImage
                ? iconImage
                : ((await (async () => {
                      const { native_path } = await getDefaultIconPath(defaultIcon);

                      const iconImage = await Image.fromPath(native_path);

                      return iconImage;
                  })()) ??
                  (await defaultWindowIcon()) ??
                  ''),
            showMenuOnLeftClick: false,
            tooltip: 'Snow Shot',
            action: (event) => {
                switch (event.type) {
                    case 'Click':
                        if (event.button === 'Left') {
                            if (
                                getAppSettings()[AppSettingsGroup.FunctionTrayIcon]
                                    .iconClickAction === TrayIconClickAction.Screenshot
                            ) {
                                executeScreenshot();
                            } else if (
                                getAppSettings()[AppSettingsGroup.FunctionTrayIcon]
                                    .iconClickAction === TrayIconClickAction.ShowMainWindow
                            ) {
                                showWindow();
                            }
                        }
                        break;
                }
            },
            menu: await Menu.new({
                id: `${appWindow.label}-trayIconMenu`,
                items: [
                    {
                        id: `${appWindow.label}-screenshot`,
                        text: intl.formatMessage({ id: 'home.screenshot' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.Screenshot].shortcutKey),
                        action: async () => {
                            executeScreenshot();
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-delay`,
                        text: intl.formatMessage(
                            {
                                id: 'home.screenshotFunction.screenshotDelay',
                            },
                            {
                                seconds: intl.formatMessage(
                                    {
                                        id: 'home.screenshotFunction.screenshotDelay.seconds',
                                    },
                                    {
                                        seconds: delayScreenshotSeconds,
                                    },
                                ),
                            },
                        ),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.ScreenshotDelay].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.Delay);
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-fixedTool`,
                        text: intl.formatMessage({ id: 'draw.fixedTool' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.ScreenshotFixed].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.Fixed);
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-ocr`,
                        text: intl.formatMessage({ id: 'draw.ocrDetectTool' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.ScreenshotOcr].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.OcrDetect);
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-ocr-translate`,
                        text: intl.formatMessage({ id: 'draw.ocrTranslateTool' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(
                                  shortcutKeys[AppFunction.ScreenshotOcrTranslate].shortcutKey,
                              ),
                        action: async () => {
                            executeScreenshot(ScreenshotType.OcrTranslate);
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-copy`,
                        text: intl.formatMessage({
                            id: 'home.screenshotFunction.screenshotCopy',
                        }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.ScreenshotCopy].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.Copy);
                        },
                    },
                    ...(shortcutKeys[AppFunction.ScreenshotFocusedWindow].shortcutKey
                        ? [
                              {
                                  id: `${appWindow.label}-screenshot-focused-window`,
                                  text: intl.formatMessage({
                                      id: 'home.screenshotFunction.screenshotFocusedWindow',
                                  }),
                                  accelerator: disableShortcut
                                      ? undefined
                                      : formatKey(
                                            shortcutKeys[AppFunction.ScreenshotFocusedWindow]
                                                .shortcutKey,
                                        ),
                                  action: async () => {
                                      executeScreenshotFocusedWindow(getAppSettings());
                                  },
                              },
                          ]
                        : []),
                    {
                        id: `${appWindow.label}-screenshot-fullScreen`,
                        text: intl.formatMessage({
                            id: 'home.screenshotFunction.screenshotFullScreen',
                        }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.ScreenshotFullScreen].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.CaptureFullScreen);
                        },
                    },
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-chat`,
                        text: intl.formatMessage({ id: 'home.chat' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.Chat].shortcutKey),
                        action: async () => {
                            executeChat();
                        },
                    },
                    ...(shortcutKeys[AppFunction.ChatSelectText].shortcutKey
                        ? [
                              {
                                  id: `${appWindow.label}-chat-selectText`,
                                  text: intl.formatMessage({ id: 'home.chatSelectText' }),
                                  accelerator: disableShortcut
                                      ? undefined
                                      : formatKey(
                                            shortcutKeys[AppFunction.ChatSelectText].shortcutKey,
                                        ),
                                  action: async () => {
                                      executeChatSelectedText();
                                  },
                              },
                          ]
                        : []),
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-translation`,
                        text: intl.formatMessage({ id: 'home.translation' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.Translation].shortcutKey),
                        action: async () => {
                            executeTranslate();
                        },
                    },
                    ...(shortcutKeys[AppFunction.TranslationSelectText].shortcutKey
                        ? [
                              {
                                  id: `${appWindow.label}-translation-selectText`,
                                  text: intl.formatMessage({
                                      id: 'home.translationSelectText',
                                  }),
                                  accelerator: disableShortcut
                                      ? undefined
                                      : formatKey(
                                            shortcutKeys[AppFunction.TranslationSelectText]
                                                .shortcutKey,
                                        ),
                                  action: async () => {
                                      executeTranslateSelectedText();
                                  },
                              },
                          ]
                        : []),
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-screenshot-videoRecord`,
                        text: intl.formatMessage({ id: 'draw.extraTool.videoRecord' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.VideoRecord].shortcutKey),
                        action: async () => {
                            executeScreenshot(ScreenshotType.VideoRecord);
                        },
                    },
                    {
                        id: `${appWindow.label}-screenshot-videoRecord-copy`,
                        text: intl.formatMessage({ id: 'home.videoRecordFunction.copyVideo' }),
                        action: async () => {
                            startOrCopyVideo();
                        },
                    },
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-screenshot-fixedContent`,
                        text: intl.formatMessage({ id: 'home.fixedContent' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.FixedContent].shortcutKey),
                        action: async () => {
                            createFixedContentWindow();
                        },
                    },
                    ...getPlatformValue(
                        [
                            {
                                id: `${appWindow.label}-screenshot-topWindow`,
                                text: intl.formatMessage({ id: 'home.topWindow' }),
                                accelerator: disableShortcut
                                    ? undefined
                                    : formatKey(shortcutKeys[AppFunction.TopWindow].shortcutKey),
                                action: async () => {
                                    executeScreenshot(ScreenshotType.TopWindow);
                                },
                            },
                        ],
                        [],
                    ),
                    {
                        id: `${appWindow.label}-screenshot-fullScreenDraw`,
                        text: intl.formatMessage({ id: 'home.fullScreenDraw' }),
                        accelerator: disableShortcut
                            ? undefined
                            : formatKey(shortcutKeys[AppFunction.FullScreenDraw].shortcutKey),
                        action: async () => {
                            createFullScreenDrawWindow();
                        },
                    },
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-disableShortcut`,
                        text: intl.formatMessage({ id: 'home.disableShortcut' }),
                        checked: disableShortcut,
                        action: async () => {
                            setTrayIconState({
                                disableShortcut: !disableShortcut,
                            });
                        },
                    },
                    {
                        id: `${appWindow.label}-show-main-window`,
                        text: intl.formatMessage({ id: 'home.showMainWindow' }),
                        action: async () => {
                            showWindow();
                        },
                    },
                    {
                        item: 'Separator',
                    },
                    {
                        id: `${appWindow.label}-exit`,
                        text: intl.formatMessage({ id: 'home.exit' }),
                        action: async () => {
                            exitApp();
                        },
                    },
                ],
            }),
        };

        try {
            await closeTrayIcon();
            trayIcon = await TrayIcon.new(options);
        } catch (error) {
            appError(`[initTrayIcon] create tray icon failed`, error);
            message.error(intl.formatMessage({ id: 'home.trayIcon.error' }));
        }
    }, [
        shortcutKeys,
        enableTrayIcon,
        intl,
        disableShortcut,
        delayScreenshotSeconds,
        closeTrayIcon,
        iconPath,
        message,
        defaultIcon,
        getAppSettings,
        setTrayIconState,
    ]);

    useEffect(() => {
        initTrayIcon();

        const handleBeforeUnload = async () => {
            await closeTrayIcon();
        };

        window.addEventListener('beforeunload', handleBeforeUnload);

        return () => {
            handleBeforeUnload();
            window.removeEventListener('beforeunload', handleBeforeUnload);
        };
    }, [closeTrayIcon, initTrayIcon]);

    return null;
};

export const TrayIconLoader = React.memo(TrayIconLoaderComponent);
