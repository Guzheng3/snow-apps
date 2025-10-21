import { defaultWindowIcon } from "@tauri-apps/api/app";
import { convertFileSrc } from "@tauri-apps/api/core";
import { Image } from "@tauri-apps/api/image";
import { Menu, type MenuItem } from "@tauri-apps/api/menu";
import { join, resourceDir } from "@tauri-apps/api/path";
import { TrayIcon, type TrayIconOptions } from "@tauri-apps/api/tray";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { debounce, isEqual } from "es-toolkit";
import React, {
	useCallback,
	useContext,
	useEffect,
	useMemo,
	useRef,
	useState,
} from "react";
import { useIntl } from "react-intl";
import { exitApp } from "@/commands";
import {
	createFixedContentWindow,
	createFullScreenDrawWindow,
} from "@/commands/core";
import {
	PLUGIN_ID_AI_CHAT,
	PLUGIN_ID_FFMPEG,
	PLUGIN_ID_RAPID_OCR,
} from "@/constants/pluginService";
import { AntdContext } from "@/contexts/antdContext";
import { AppSettingsPublisher } from "@/contexts/appSettingsActionContext";
import { usePluginServiceContext } from "@/contexts/pluginServiceContext";
import {
	executeScreenshot,
	executeScreenshotFocusedWindow,
} from "@/functions/screenshot";
import { sendErrorMessage } from "@/functions/sendMessage";
import {
	executeChat,
	executeChatSelectedText,
	executeTranslate,
	executeTranslateSelectedText,
} from "@/functions/tools";
import { startOrCopyVideo } from "@/functions/videoRecord";
import { useAppSettingsLoad } from "@/hooks/useAppSettingsLoad";
import { createPublisher } from "@/hooks/useStatePublisher";
import { useStateRef } from "@/hooks/useStateRef";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import {
	type AppSettingsData,
	AppSettingsGroup,
	TrayIconClickAction,
	TrayIconDefaultIcon,
} from "@/types/appSettings";
import {
	AppFunction,
	type AppFunctionConfig,
} from "@/types/components/appFunction";
import { formatKey } from "@/utils/format";
import { appError } from "@/utils/log";
import { getPlatformValue } from "@/utils/platform";
import { ScreenshotType } from "@/utils/types";
import { showWindow } from "@/utils/window";

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

	const nativePath = await join(
		basePath,
		"app-icons",
		`snow-shot-tray-${defaultIcon}.png`,
	);
	const defaultIconPath = convertFileSrc(nativePath);

	return {
		web_path: defaultIconPath,
		native_path: nativePath,
	};
};

const TrayIconLoaderComponent = () => {
	const intl = useIntl();
	const { message } = useContext(AntdContext);
	const [disableShortcut, _setDisableShortcut] = useState(false);
	const [, setTrayIconState] = useStateSubscriber(
		TrayIconStatePublisher,
		useCallback((state: { disableShortcut: boolean }) => {
			_setDisableShortcut(state.disableShortcut);
		}, []),
	);

	const [delayScreenshotSeconds, setDelayScreenshotSeconds] = useState(0);
	const [shortcutKeys, setShortcutKeys, shortcutKeysRef] = useStateRef<
		Record<AppFunction, AppFunctionConfig> | undefined
	>(undefined);
	const [iconPath, setIconPath] = useState("");
	const [defaultIcon, setDefaultIcon] = useState<TrayIconDefaultIcon>(
		TrayIconDefaultIcon.Default,
	);
	const [enableTrayIcon, setEnableTrayIcon] = useState(false);
	const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
	useAppSettingsLoad(
		useCallback(
			(settings: AppSettingsData, previous: AppSettingsData | undefined) => {
				if (
					shortcutKeysRef.current === undefined ||
					!isEqual(
						settings[AppSettingsGroup.AppFunction],
						previous?.[AppSettingsGroup.AppFunction],
					)
				) {
					setShortcutKeys(settings[AppSettingsGroup.AppFunction]);
				}

				setIconPath(settings[AppSettingsGroup.CommonTrayIcon].iconPath);
				setDefaultIcon(settings[AppSettingsGroup.CommonTrayIcon].defaultIcons);
				setEnableTrayIcon(
					settings[AppSettingsGroup.CommonTrayIcon].enableTrayIcon,
				);
				setDelayScreenshotSeconds(
					settings[AppSettingsGroup.Cache].delayScreenshotSeconds,
				);
			},
			[setShortcutKeys, shortcutKeysRef],
		),
		true,
	);

	const { isReadyStatus } = usePluginServiceContext();
	const initTrayIcon = useCallback(async (): Promise<
		| {
				trayIcon: TrayIcon | undefined;
				trayIconMenu: Menu | undefined;
		  }
		| undefined
	> => {
		if (!isReadyStatus) {
			return;
		}

		if (!shortcutKeys) {
			return;
		}

		if (!enableTrayIcon) {
			return;
		}

		const appWindow = getCurrentWindow();

		let iconImage: Image | undefined;
		try {
			if (iconPath) {
				iconImage = await Image.fromPath(iconPath);
			}
		} catch {
			message.error(intl.formatMessage({ id: "home.trayIcon.error4" }));
			return;
		}

		if (iconImage) {
			const size = await iconImage.size();
			if (size.width > 128 || size.height > 128) {
				message.error(intl.formatMessage({ id: "home.trayIcon.error3" }));
				return;
			}
		}

		const menu = await Menu.new({
			id: `${appWindow.label}-trayIconMenu`,
			items: [
				{
					id: `${appWindow.label}-screenshot`,
					text: intl.formatMessage({ id: "home.screenshot" }),
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
							id: "home.screenshotFunction.screenshotDelay",
						},
						{
							seconds: intl.formatMessage(
								{
									id: "home.screenshotFunction.screenshotDelay.seconds",
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
					text: intl.formatMessage({ id: "draw.fixedTool" }),
					accelerator: disableShortcut
						? undefined
						: formatKey(shortcutKeys[AppFunction.ScreenshotFixed].shortcutKey),
					action: async () => {
						executeScreenshot(ScreenshotType.Fixed);
					},
				},
				...(isReadyStatus(PLUGIN_ID_RAPID_OCR)
					? [
							{
								id: `${appWindow.label}-screenshot-ocr`,
								text: intl.formatMessage({ id: "draw.ocrDetectTool" }),
								accelerator: disableShortcut
									? undefined
									: formatKey(
											shortcutKeys[AppFunction.ScreenshotOcr].shortcutKey,
										),
								action: async () => {
									executeScreenshot(ScreenshotType.OcrDetect);
								},
							},
							{
								id: `${appWindow.label}-screenshot-ocr-translate`,
								text: intl.formatMessage({ id: "draw.ocrTranslateTool" }),
								accelerator: disableShortcut
									? undefined
									: formatKey(
											shortcutKeys[AppFunction.ScreenshotOcrTranslate]
												.shortcutKey,
										),
								action: async () => {
									executeScreenshot(ScreenshotType.OcrTranslate);
								},
							},
						]
					: []),
				{
					id: `${appWindow.label}-screenshot-copy`,
					text: intl.formatMessage({
						id: "home.screenshotFunction.screenshotCopy",
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
									id: "home.screenshotFunction.screenshotFocusedWindow",
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
						id: "home.screenshotFunction.screenshotFullScreen",
					}),
					accelerator: disableShortcut
						? undefined
						: formatKey(
								shortcutKeys[AppFunction.ScreenshotFullScreen].shortcutKey,
							),
					action: async () => {
						executeScreenshot(ScreenshotType.CaptureFullScreen);
					},
				},
				...(isReadyStatus(PLUGIN_ID_AI_CHAT)
					? [
							{
								item: "Separator",
							} as unknown as MenuItem,
							{
								id: `${appWindow.label}-chat`,
								text: intl.formatMessage({ id: "home.chat" }),
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
											text: intl.formatMessage({ id: "home.chatSelectText" }),
											accelerator: disableShortcut
												? undefined
												: formatKey(
														shortcutKeys[AppFunction.ChatSelectText]
															.shortcutKey,
													),
											action: async () => {
												executeChatSelectedText();
											},
										},
									]
								: []),
						]
					: []),
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-translation`,
					text: intl.formatMessage({ id: "home.translation" }),
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
									id: "home.translationSelectText",
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
				...(isReadyStatus(PLUGIN_ID_FFMPEG)
					? [
							{
								item: "Separator",
							} as unknown as MenuItem,
						]
					: []),
				...(isReadyStatus(PLUGIN_ID_FFMPEG)
					? [
							{
								id: `${appWindow.label}-screenshot-videoRecord`,
								text: intl.formatMessage({
									id: "draw.extraTool.videoRecord",
								}),
								accelerator: disableShortcut
									? undefined
									: formatKey(
											shortcutKeys[AppFunction.VideoRecord].shortcutKey,
										),
								action: async () => {
									executeScreenshot(ScreenshotType.VideoRecord);
								},
							},
							{
								id: `${appWindow.label}-screenshot-videoRecord-copy`,
								text: intl.formatMessage({
									id: "home.videoRecordFunction.copyVideo",
								}),
								action: async () => {
									startOrCopyVideo();
								},
							},
						]
					: []),
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-screenshot-fixedContent`,
					text: intl.formatMessage({ id: "home.fixedContent" }),
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
							text: intl.formatMessage({ id: "home.topWindow" }),
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
					text: intl.formatMessage({ id: "home.fullScreenDraw" }),
					accelerator: disableShortcut
						? undefined
						: formatKey(shortcutKeys[AppFunction.FullScreenDraw].shortcutKey),
					action: async () => {
						createFullScreenDrawWindow();
					},
				},
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-disableShortcut`,
					text: intl.formatMessage({ id: "home.disableShortcut" }),
					checked: disableShortcut,
					action: async () => {
						setTrayIconState({
							disableShortcut: !disableShortcut,
						});
					},
				},
				{
					id: `${appWindow.label}-show-main-window`,
					text: intl.formatMessage({ id: "home.showMainWindow" }),
					action: async () => {
						showWindow();
					},
				},
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-exit`,
					text: intl.formatMessage({ id: "home.exit" }),
					action: async () => {
						exitApp();
					},
				},
			],
		});

		const options: TrayIconOptions = {
			icon: iconImage
				? iconImage
				: ((await (async () => {
						const { native_path } = await getDefaultIconPath(defaultIcon);

						const iconImage = await Image.fromPath(native_path);

						return iconImage;
					})()) ??
					(await defaultWindowIcon()) ??
					""),
			showMenuOnLeftClick: false,
			tooltip: "Snow Shot",
			action: (event) => {
				switch (event.type) {
					case "Click":
						if (event.button === "Left") {
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
			menu,
		};

		return {
			trayIcon: await TrayIcon.new(options),
			trayIconMenu: menu,
		};
	}, [
		shortcutKeys,
		enableTrayIcon,
		intl,
		disableShortcut,
		delayScreenshotSeconds,
		iconPath,
		message,
		defaultIcon,
		getAppSettings,
		setTrayIconState,
		isReadyStatus,
	]);

	useEffect(() => {
		if (!isReadyStatus) {
			return;
		}

		if (!shortcutKeys) {
			return;
		}

		const trayIconPromise = initTrayIcon();

		const handleBeforeUnload = async () => {
			trayIconPromise
				.then((trayIcon) => {
					if (trayIcon) {
						trayIcon.trayIconMenu?.close();
						trayIcon.trayIcon?.close();
					}
				})
				.catch((error) => {
					appError(`[TrayIconLoader] beforeunload event failed`, error);
				});
		};

		window.addEventListener("beforeunload", handleBeforeUnload);

		return () => {
			trayIconPromise
				.then((trayIcon) => {
					if (trayIcon) {
						trayIcon.trayIconMenu?.close();
						trayIcon.trayIcon?.close();
					}
				})
				.catch((error) => {
					appError(`[TrayIconLoader] close tray icon failed`, error);
				});

			window.removeEventListener("beforeunload", handleBeforeUnload);
		};
	}, [initTrayIcon, isReadyStatus, shortcutKeys]);

	return null;
};

export const TrayIconLoader = React.memo(TrayIconLoaderComponent);
