"use client";

import { convertFileSrc } from "@tauri-apps/api/core";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { useCallback, useEffect, useRef, useState } from "react";
import extraClipboard from "tauri-plugin-clipboard-api";
import {
	getCurrentMonitorInfo,
	type MonitorInfo,
	readImageFromClipboard,
} from "@/commands/core";
import { setDrawWindowStyle } from "@/commands/screenshot";
import {
	scrollScreenshotClear,
	scrollScreenshotGetImageData,
} from "@/commands/scrollScreenshot";
import { TextScaleFactorContextProvider } from "@/components/textScaleFactorContextProvider";
import { useAppSettingsLoad } from "@/hooks/useAppSettingsLoad";
import {
	AppSettingsFixedContentInitialPosition,
	AppSettingsGroup,
} from "@/types/appSettings";
import { setWindowRect, showWindow } from "@/utils/window";
import {
	getImageBufferFromSharedBuffer,
	type ImageSharedBufferData,
} from "../draw/tools";
import {
	type FixedContentActionType,
	FixedContentCore,
} from "./components/fixedContentCore";

export const FixedContentPage: React.FC = () => {
	const fixedContentActionRef = useRef<FixedContentActionType>(undefined);

	const initedRef = useRef(false);

	const [windowInitialPosition, setWindowInitialPosition] = useState<
		undefined | AppSettingsFixedContentInitialPosition
	>();
	useAppSettingsLoad(
		useCallback((settings) => {
			setWindowInitialPosition(
				settings[AppSettingsGroup.FunctionFixedContent].initialPosition,
			);
		}, []),
	);

	const init = useCallback(async () => {
		const urlParams = new URLSearchParams(window.location.search);

		if (urlParams.get("scroll_screenshot") === "true") {
			// 可能通过 SharedBuffer 传递
			const imageSharedBufferPromise =
				getImageBufferFromSharedBuffer("scroll_screenshot");
			let imageData: ArrayBuffer | ImageSharedBufferData | undefined =
				await scrollScreenshotGetImageData();
			scrollScreenshotClear();
			if (
				imageData &&
				imageData.byteLength === 1 &&
				new Uint8Array(imageData)[0] === 1
			) {
				imageData = await imageSharedBufferPromise;
			}

			if (!imageData) {
				getCurrentWindow().close();
				return;
			}

			fixedContentActionRef.current?.init({ imageContent: imageData });
		} else {
			let hasInit = false;

			const imageSharedBufferPromise = getImageBufferFromSharedBuffer(
				"read_image_from_clipboard",
			);

			const readImagePromise = readImageFromClipboard()
				.then(async (result) => {
					if (hasInit) {
						return;
					}

					let imageData: ArrayBuffer | ImageSharedBufferData | undefined =
						result;
					if (
						imageData &&
						imageData.byteLength === 1 &&
						new Uint8Array(imageData)[0] === 1
					) {
						imageData = await imageSharedBufferPromise;
					}

					if (!imageData) {
						return;
					}

					hasInit = true;
					fixedContentActionRef.current?.init({
						imageContent: imageData,
					});
				})
				.catch(() => {});

			const readHtmlPromise = extraClipboard
				.readHtml()
				.then((htmlContent) => {
					if (hasInit) {
						return;
					}

					hasInit = true;
					fixedContentActionRef.current?.init({ htmlContent });
				})
				.catch(() => {});

			const readTextPromise = extraClipboard
				.readText()
				.then((textContent) => {
					if (hasInit) {
						return;
					}

					hasInit = true;
					fixedContentActionRef.current?.init({ textContent });
				})
				.catch(() => {});

			const readFilesURIsPromise = extraClipboard
				.readFilesURIs()
				.then((fileUris) => {
					if (hasInit) {
						return;
					}

					let imageFileUri: string | undefined;
					for (const fileUri of fileUris) {
						if (
							fileUri.endsWith(".png") ||
							fileUri.endsWith(".jpg") ||
							fileUri.endsWith(".jpeg") ||
							fileUri.endsWith(".webp")
						) {
							imageFileUri = fileUri;
							break;
						}
					}

					if (!imageFileUri) {
						return;
					}

					hasInit = true;
					fixedContentActionRef.current?.init({
						imageContent: convertFileSrc(imageFileUri),
					});
				})
				.catch(() => {});

			await Promise.all([
				readImagePromise,
				readHtmlPromise,
				readTextPromise,
				readFilesURIsPromise,
			]);

			if (hasInit) {
				return;
			}

			getCurrentWindow().close();
		}
	}, []);

	useEffect(() => {
		if (initedRef.current) {
			return;
		}

		initedRef.current = true;

		init();
	}, [init]);

	const [loadParams, setLoadParams] = useState<
		| {
				container:
					| { width: number; height: number }
					| null
					| { naturalWidth: number; naturalHeight: number }
					| HTMLDivElement;
				monitorInfo?: MonitorInfo;
				initialScale?: number;
		  }
		| undefined
	>();
	const onHtmlTextImageLoad = useCallback(
		async (
			container:
				| { width: number; height: number }
				| null
				| { naturalWidth: number; naturalHeight: number }
				| HTMLDivElement,
			monitorInfo?: MonitorInfo,
			initialScale?: number,
		) => {
			setLoadParams({ container, monitorInfo, initialScale });
		},
		[],
	);

	useEffect(() => {
		if (!loadParams || !windowInitialPosition) {
			return;
		}

		const { container } = loadParams;

		const initialScale = loadParams.initialScale ?? 1;

		(async () => {
			const appWindow = getCurrentWindow();

			if (!container) {
				return;
			}

			const monitorInfo =
				loadParams.monitorInfo ?? (await getCurrentMonitorInfo());

			let width = 0;
			let height = 0;
			if ("naturalWidth" in container) {
				width = container.naturalWidth;
				height = container.naturalHeight;
			} else if ("width" in container && "height" in container) {
				width = container.width;
				height = container.height;
			} else {
				width = container.clientWidth * window.devicePixelRatio;
				height = container.clientHeight * window.devicePixelRatio;
			}

			if (width > 0 && height > 0) {
				const windowWidth = Math.floor(width * initialScale);
				const windowHeight = Math.floor(height * initialScale);

				let targetX = monitorInfo.monitor_x + monitorInfo.mouse_x;
				let targetY = monitorInfo.monitor_y + monitorInfo.mouse_y;
				if (
					windowInitialPosition ===
					AppSettingsFixedContentInitialPosition.MonitorCenter
				) {
					targetX = monitorInfo.monitor_x + monitorInfo.monitor_width / 2;
					targetY = monitorInfo.monitor_y + monitorInfo.monitor_height / 2;
				}

				const windowX = Math.round(targetX - windowWidth / 2);
				const windowY = Math.round(targetY - windowHeight / 2);
				await setWindowRect(appWindow, {
					min_x: windowX,
					min_y: windowY,
					max_x: windowX + windowWidth,
					max_y: windowY + windowHeight,
				});
				showWindow();
				setDrawWindowStyle();
			} else {
				await appWindow.close();
			}
		})();
	}, [loadParams, windowInitialPosition]);

	return (
		<TextScaleFactorContextProvider>
			<FixedContentCore
				actionRef={fixedContentActionRef}
				onHtmlLoad={onHtmlTextImageLoad}
				onTextLoad={onHtmlTextImageLoad}
				onImageLoad={onHtmlTextImageLoad}
			/>
		</TextScaleFactorContextProvider>
	);
};
