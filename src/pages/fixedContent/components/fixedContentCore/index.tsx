import { CloseOutlined, EditOutlined } from "@ant-design/icons";
import type { ExcalidrawElement } from "@mg-chao/excalidraw/element/types";
import { PhysicalPosition, PhysicalSize } from "@tauri-apps/api/dpi";
import { Menu, type MenuItemOptions, Submenu } from "@tauri-apps/api/menu";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import {
	type Window as AppWindow,
	getCurrentWindow,
} from "@tauri-apps/api/window";
import * as dialog from "@tauri-apps/plugin-dialog";
import { openUrl } from "@tauri-apps/plugin-opener";
import * as TWEEN from "@tweenjs/tween.js";
import { Button, Descriptions, Space, Typography, theme } from "antd";
import Color from "color";
import { toCanvas as htmlToCanvas } from "html-to-image";
import {
	useCallback,
	useContext,
	useEffect,
	useImperativeHandle,
	useMemo,
	useRef,
	useState,
} from "react";
import { isHotkeyPressed, useHotkeys } from "react-hotkeys-hook";
import { FormattedMessage, useIntl } from "react-intl";
import { getMousePosition, saveFile } from "@/commands";
import {
	getCurrentMonitorInfo,
	type MonitorInfo,
	setCurrentWindowAlwaysOnTop,
	startFreeDrag,
} from "@/commands/core";
import { setDrawWindowStyle } from "@/commands/screenshot";
import { INIT_CONTAINER_KEY } from "@/components/imageLayer/actions";
import { PLUGIN_ID_RAPID_OCR } from "@/constants/pluginService";
import { AntdContext } from "@/contexts/antdContext";
import { AppSettingsPublisher } from "@/contexts/appSettingsActionContext";
import { usePluginServiceContext } from "@/contexts/pluginServiceContext";
import {
	fixedContentFocusModeCloseAllWindow,
	fixedContentFocusModeCloseOtherWindow,
	fixedContentFocusModeHideOtherWindow,
	fixedContentFocusModeShowAllWindow,
} from "@/functions/fixedContent";
import { useCallbackRender } from "@/hooks/useCallbackRender";
import { useStateRef } from "@/hooks/useStateRef";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import { useTempInfo } from "@/hooks/useTempInfo";
import { useTextScaleFactor } from "@/hooks/useTextScaleFactor";
import { copyToClipboard as copyToClipboardDrawAction } from "@/pages/draw/actions";
import type { SelectRectParams } from "@/pages/draw/components/selectLayer";
import type { CaptureBoundingBoxInfo } from "@/pages/draw/extra";
import { type AppSettingsData, AppSettingsGroup } from "@/types/appSettings";
import {
	CommonKeyEventKey,
	type CommonKeyEventValue,
} from "@/types/core/commonKeyEvent";
import { ImageFormat } from "@/types/utils/file";
import {
	writeHtmlToClipboard,
	writeImageToClipboard,
	writeTextToClipboard,
} from "@/utils/clipboard";
import { generateImageFileName } from "@/utils/file";
import { formatKey } from "@/utils/format";
import { appError } from "@/utils/log";
import { MousePosition } from "@/utils/mousePosition";
import { TweenAnimation } from "@/utils/tweenAnimation";
import { closeWindowComplete } from "@/utils/window";
import { zIndexs } from "@/utils/zIndex";
import {
	type AppOcrResult,
	OcrResult,
	type OcrResultActionType,
	type OcrResultInitDrawCanvasParams,
} from "../ocrResult";
import { renderToCanvasAction } from "./actions";
import {
	DrawLayer,
	type FixedContentCoreDrawActionType,
} from "./components/drawLayer";
import { HandleFocusMode } from "./components/handleFocusMode";
import {
	FixedContentImageLayer,
	type FixedContentImageLayerActionType,
} from "./components/imageLayer";
import { ResizeWindow } from "./components/resizeWindow";
import { getHtmlContent, getStyleProps } from "./extra";

export type FixedContentInitDrawParams = {
	captureBoundingBoxInfo: CaptureBoundingBoxInfo;
	canvas: HTMLCanvasElement;
	drawElements: {
		scrollX: number;
		scrollY: number;
		zoom: number;
		elements: ExcalidrawElement[];
	};
	windowDevicePixelRatio: number;
	/** 已有的 OCR 结果 */
	ocrResult: AppOcrResult | undefined;
	/** 选择区域参数 */
	selectRectParams: SelectRectParams;
};

export type FixedContentInitHtmlParams = {
	htmlContent: string;
};

export type FixedContentInitTextParams = {
	textContent: string;
};

export type FixedContentInitImageParams = {
	imageContent: Blob | string;
};

export type FixedContentActionType = {
	init: (
		params:
			| FixedContentInitDrawParams
			| FixedContentInitHtmlParams
			| FixedContentInitTextParams
			| FixedContentInitImageParams,
	) => Promise<void>;
};

export enum FixedContentType {
	DrawCanvas = "drawCanvas",
	Html = "html",
	Text = "text",
	Image = "image",
}

let rightClickMenu: Menu | undefined;
const closeRightClickMenu = async () => {
	try {
		await rightClickMenu?.close();
		rightClickMenu = undefined;
	} catch (error) {
		console.log("[closeRightClickMenu] failed to close menu", error);
	}
};

const getSelectTextMode = (fixedContentType: FixedContentType | undefined) => {
	if (!fixedContentType) {
		return undefined;
	}

	if (
		fixedContentType === FixedContentType.DrawCanvas ||
		fixedContentType === FixedContentType.Image
	) {
		return "ocr"; // 使用 OCR 选取文本
	}
	return "text"; // 支持文本选取
};

export type FixedContentWindowSize = {
	width: number;
	height: number;
};

export enum FixedContentScrollAction {
	Zoom = "zoom",
	RotateX = "rotateX",
	RotateY = "rotateY",
	RotateZ = "rotateZ",
}

export type FixedContentProcessImageConfig = {
	angle: number;
	horizontalFlip: boolean;
	verticalFlip: boolean;
};

export const SCALE_WINDOW_MAX_SCALE = 200;
export const SCALE_WINDOW_MIN_SCALE = 20;

export const FixedContentCore: React.FC<{
	actionRef: React.RefObject<FixedContentActionType | undefined>;
	onDrawLoad?: () => void;
	onHtmlLoad?: ({ width, height }: { width: number; height: number }) => void;
	onTextLoad?: (container: HTMLDivElement | null) => void;
	onImageLoad?: (
		container: { naturalWidth: number; naturalHeight: number },
		monitorInfo: MonitorInfo,
	) => void;
	disabled?: boolean;
}> = ({
	actionRef,
	onDrawLoad,
	onHtmlLoad,
	onTextLoad,
	onImageLoad,
	disabled,
}) => {
	const intl = useIntl();
	const { token } = theme.useToken();
	const { message } = useContext(AntdContext);
	const [hotkeys, setHotkeys] = useState<
		Record<CommonKeyEventKey, CommonKeyEventValue> | undefined
	>(undefined);
	const [fixedBorderColor, setFixedBorderColor] = useState<string | undefined>(
		undefined,
	);
	useStateSubscriber(
		AppSettingsPublisher,
		useCallback((settings: AppSettingsData) => {
			setFixedBorderColor(settings[AppSettingsGroup.FixedContent].borderColor);
			setHotkeys(settings[AppSettingsGroup.CommonKeyEvent]);
		}, []),
	);

	const appWindowRef = useRef<AppWindow | undefined>(undefined);
	useEffect(() => {
		appWindowRef.current = getCurrentWindow();
	}, []);

	const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
	const ocrResultActionRef = useRef<OcrResultActionType>(undefined);
	const [windowSize, setWindowSize, windowSizeRef] =
		useStateRef<FixedContentWindowSize>({
			width: 0,
			height: 0,
		});
	const canvasPropsRef = useRef<{
		width: number;
		height: number;
		scaleFactor: number;
		ignoreTextScaleFactor?: boolean;
	}>({
		width: 0,
		height: 0,
		scaleFactor: 1,
		ignoreTextScaleFactor: false,
	});
	const canvasElementRef = useRef<HTMLCanvasElement | undefined>(undefined);
	const imageUrlRef = useRef<string | undefined>(undefined);
	const [scale, setScale, scaleRef] = useStateRef<{
		x: number;
		y: number;
	}>({
		x: 100,
		y: 100,
	});

	const [fixedContentType, setFixedContentType, fixedContentTypeRef] =
		useStateRef<FixedContentType | undefined>(undefined);
	const [showBorder, setShowBorder] = useState(true);
	const [borderRadius, setBorderRadius] = useState(0);
	const [enableDraw, setEnableDraw, enableDrawRef] = useStateRef(false);
	const [enableDrawLayer, setEnableDrawLayer] = useState(false);
	const [enableSelectText, setEnableSelectText, enableSelectTextRef] =
		useStateRef(false);
	const [contentOpacity, setContentOpacity, contentOpacityRef] = useStateRef(1);
	const [isAlwaysOnTop, setIsAlwaysOnTop] = useStateRef(true);
	const dragRegionMouseDownMousePositionRef = useRef<MousePosition>(undefined);

	const [textContent, setTextContent, textContentRef] = useStateRef<
		| {
				content: string;
				colorText:
					| {
							color: string;
							rgb: string;
							hex: string;
							hsl: string;
					  }
					| undefined;
		  }
		| undefined
	>(undefined);
	const textContentContainerRef = useRef<HTMLDivElement>(null);

	const [textScaleFactor, , textScaleFactorRef] = useTextScaleFactor();
	const contentScaleFactor = useMemo(() => {
		if (
			fixedContentType === FixedContentType.DrawCanvas ||
			fixedContentType === FixedContentType.Image
		) {
			return textScaleFactor;
		}
		return 1;
	}, [fixedContentType, textScaleFactor]);

	const [processImageConfig, setProcessImageConfig, processImageConfigRef] =
		useStateRef<FixedContentProcessImageConfig>({
			angle: 0,
			horizontalFlip: false,
			verticalFlip: false,
		});
	const renderToCanvas = useCallback(
		async (ignoreDrawCanvas: boolean = false) => {
			return renderToCanvasAction(
				imageLayerActionRef,
				drawActionRef,
				processImageConfigRef,
				ignoreDrawCanvas,
				{
					min_x: 0,
					min_y: 0,
					max_x: canvasPropsRef.current.width,
					max_y: canvasPropsRef.current.height,
				},
			);
		},
		[processImageConfigRef],
	);

	const copyRawToClipboard = useCallback(async () => {
		if (
			fixedContentTypeRef.current === FixedContentType.DrawCanvas ||
			fixedContentTypeRef.current === FixedContentType.Image
		) {
			const imageLayerAction =
				imageLayerActionRef.current?.getImageLayerAction();
			if (!imageLayerAction) {
				return;
			}
			const baseImageBuffer = await imageLayerAction.renderToPng(
				{
					min_x: 0,
					min_y: 0,
					max_x: canvasPropsRef.current.width,
					max_y: canvasPropsRef.current.height,
				},
				INIT_CONTAINER_KEY,
			);

			if (!baseImageBuffer) {
				return;
			}

			await writeImageToClipboard(baseImageBuffer);
		} else if (
			fixedContentTypeRef.current === FixedContentType.Html &&
			originHtmlContentRef.current
		) {
			await writeHtmlToClipboard(originHtmlContentRef.current);
		} else if (
			fixedContentTypeRef.current === FixedContentType.Text &&
			textContentRef.current
		) {
			await writeTextToClipboard(textContentRef.current.content);
		}
	}, [fixedContentTypeRef, textContentRef]);

	const hasInitImageLayerRef = useRef(false);
	const tryInitImageLayer = useCallback(
		async (isHtmlTextLoad: boolean = false) => {
			if (hasInitImageLayerRef.current) {
				return;
			}

			if (!imageLayerActionRef.current) {
				return;
			}

			if (fixedContentTypeRef.current === FixedContentType.DrawCanvas) {
				if (!canvasElementRef.current) {
					return;
				}
				hasInitImageLayerRef.current = true;

				await imageLayerActionRef.current.initImageLayer(
					canvasPropsRef.current.width,
					canvasPropsRef.current.height,
				);

				const context = canvasElementRef.current.getContext("2d");
				if (!context) {
					return;
				}
				const imageData = context.getImageData(
					0,
					0,
					canvasElementRef.current.width,
					canvasElementRef.current.height,
				);
				await imageLayerActionRef.current.setBaseImage(imageData);

				// 清除 canvas 的数据
				canvasElementRef.current = undefined;

				if (
					getAppSettings()[AppSettingsGroup.FunctionFixedContent]
						.autoCopyToClipboard
				) {
					copyRawToClipboard();
				}
			} else if (fixedContentTypeRef.current === FixedContentType.Image) {
				if (!imageUrlRef.current) {
					return;
				}
				hasInitImageLayerRef.current = true;

				const baseImageSize =
					await imageLayerActionRef.current.initBaseImageTexture(
						imageUrlRef.current,
					);

				const monitorInfo = await getCurrentMonitorInfo();
				onImageLoad?.(
					{
						naturalWidth: baseImageSize.width,
						naturalHeight: baseImageSize.height,
					},
					monitorInfo,
				);

				const imageWidth =
					baseImageSize.width / monitorInfo.monitor_scale_factor;
				const imageHeight =
					baseImageSize.height / monitorInfo.monitor_scale_factor;

				setWindowSize({
					width: imageWidth,
					height: imageHeight,
				});
				canvasPropsRef.current = {
					width: baseImageSize.width,
					height: baseImageSize.height,
					scaleFactor: monitorInfo.monitor_scale_factor,
					ignoreTextScaleFactor: false,
				};

				await imageLayerActionRef.current.initImageLayer(
					canvasPropsRef.current.width,
					canvasPropsRef.current.height,
				);

				await imageLayerActionRef.current.setBaseImage({
					type: "base_image_texture",
				});
				// 清除 imageUrl
				URL.revokeObjectURL(imageUrlRef.current);
				imageUrlRef.current = undefined;
			} else if (
				fixedContentTypeRef.current === FixedContentType.Html ||
				fixedContentTypeRef.current === FixedContentType.Text
			) {
				if (!isHtmlTextLoad) {
					return;
				}

				if (
					!htmlContentContainerRef.current?.contentWindow?.document.body &&
					!textContentContainerRef.current
				) {
					return;
				}
				hasInitImageLayerRef.current = true;

				const sourceCanvas = await htmlToCanvas(
					htmlContentContainerRef.current?.contentWindow?.document.body ??
						textContentContainerRef.current ??
						document.body,
				);
				if (!sourceCanvas) {
					return;
				}

				copyToClipboardDrawAction(sourceCanvas, undefined, undefined);

				await imageLayerActionRef.current.initImageLayer(
					sourceCanvas.width,
					sourceCanvas.height,
				);

				const context = sourceCanvas.getContext("2d");
				if (!context) {
					return;
				}
				const imageData = context.getImageData(
					0,
					0,
					sourceCanvas.width,
					sourceCanvas.height,
				);
				await imageLayerActionRef.current.setBaseImage(imageData, true);
			}
		},
		[
			fixedContentTypeRef,
			onImageLoad,
			setWindowSize,
			copyRawToClipboard,
			getAppSettings,
		],
	);

	const [htmlContent, setHtmlContent] = useState<string | undefined>(undefined);
	const originHtmlContentRef = useRef<string | undefined>(undefined);
	const htmlContentContainerRef = useRef<HTMLIFrameElement>(null);
	const initHtml = useCallback(
		async (htmlContent: string) => {
			// 通过设置窗口大小的位置，来激活窗口，触发窗口的 laod 事件
			await getCurrentWindow().setPosition(new PhysicalPosition(0, 0));
			await Promise.all([
				getCurrentWindow().setSize(new PhysicalSize(600, 600)),
				getCurrentWebview().setSize(new PhysicalSize(600, 600)),
			]);

			originHtmlContentRef.current = htmlContent;
			const parser = new DOMParser();
			const contentHtmlDom = parser.parseFromString(htmlContent, "text/html");

			// 移除危险标签
			contentHtmlDom
				.querySelectorAll("iframe,object,embed,script,fencedframe")
				.forEach((el) => {
					el.remove();
				});

			// 移除所有危险的事件处理器属性和危险链接
			contentHtmlDom.querySelectorAll("*").forEach((el) => {
				// 移除所有 on* 事件属性
				Array.from(el.attributes).forEach((attr) => {
					if (attr.name.toLowerCase().startsWith("on")) {
						el.removeAttribute(attr.name);
					}
				});

				// 移除危险的 href 和 src 属性中的 javascript: 和 data: 协议
				["href", "src", "action", "formaction"].forEach((attrName) => {
					const attrValue = el.getAttribute(attrName);
					if (attrValue) {
						const lowerValue = attrValue.toLowerCase().trim();
						if (lowerValue.startsWith("javascript:")) {
							el.removeAttribute(attrName);
						}
					}
				});
			});

			htmlContent = contentHtmlDom.body.innerHTML;
			setFixedContentType(FixedContentType.Html);

			setHtmlContent(htmlContent);
		},
		[setFixedContentType],
	);

	const initText = useCallback(
		(textContent: string) => {
			setFixedContentType(FixedContentType.Text);

			let colorText:
				| {
						color: string;
						rgb: string;
						hex: string;
						hsl: string;
				  }
				| undefined;
			try {
				const color = Color(textContent);

				const rgbColor = color.rgb();
				const hexColor = color.hex();
				const hslColor = color.hsl();

				colorText = {
					color: color.string(),
					rgb: `${rgbColor.red()}, ${rgbColor.green()}, ${rgbColor.blue()}`,
					hex: hexColor.toString(),
					hsl: `${hslColor.hue().toFixed(1)}, ${hslColor.saturationl().toFixed(1)}, ${hslColor.lightness().toFixed(1)}`,
				};
			} catch {
				colorText = undefined;
			}

			setTextContent({
				content: textContent,
				colorText,
			});
			setTimeout(() => {
				let timeout = 0;
				if (colorText) {
					if (textContentContainerRef.current) {
						textContentContainerRef.current.style.width = "280px";
					}
				} else {
					if (
						textContentContainerRef.current &&
						textContentContainerRef.current.clientWidth >
							800 * window.devicePixelRatio
					) {
						textContentContainerRef.current.style.width = "800px";
						textContentContainerRef.current.style.whiteSpace = "normal";
						timeout = 17;
					}
				}

				setTimeout(() => {
					tryInitImageLayer(true);
					onTextLoad?.(textContentContainerRef.current);

					if (textContentContainerRef.current) {
						setWindowSize({
							width: textContentContainerRef.current.clientWidth,
							height: textContentContainerRef.current.clientHeight,
						});
						canvasPropsRef.current = {
							width:
								textContentContainerRef.current.clientWidth *
								window.devicePixelRatio,
							height:
								textContentContainerRef.current.clientHeight *
								window.devicePixelRatio,
							scaleFactor: window.devicePixelRatio,
							ignoreTextScaleFactor: true,
						};
					}
				}, timeout);
			}, 17);
		},
		[
			setFixedContentType,
			setTextContent,
			onTextLoad,
			setWindowSize,
			tryInitImageLayer,
		],
	);

	const initOcrParams = useRef<OcrResultInitDrawCanvasParams | undefined>(
		undefined,
	);

	const imageRef = useRef<HTMLImageElement>(null);
	const imageOcrSignRef = useRef<boolean>(false);
	const initImage = useCallback(
		(imageContent: Blob | string) => {
			setFixedContentType(FixedContentType.Image);

			if (typeof imageContent === "string") {
				imageUrlRef.current = imageContent;
			} else {
				imageUrlRef.current = URL.createObjectURL(imageContent);
			}

			imageOcrSignRef.current = false;

			tryInitImageLayer();
		},
		[setFixedContentType, tryInitImageLayer],
	);

	const drawActionRef = useRef<FixedContentCoreDrawActionType | undefined>(
		undefined,
	);

	const renderToBlob = useCallback(
		async (ignoreDrawCanvas: boolean = false) => {
			const canvas = await renderToCanvas(ignoreDrawCanvas);
			if (!canvas) {
				return;
			}

			return new Promise<Blob | null>((resolve) => {
				canvas.toBlob(resolve, "image/png", 1);
			});
		},
		[renderToCanvas],
	);

	const { isReady, isReadyStatus } = usePluginServiceContext();
	const selectRectParamsRef = useRef<SelectRectParams | undefined>(undefined);
	const initDrawElementsRef = useRef<
		FixedContentInitDrawParams["drawElements"] | undefined
	>(undefined);
	const initDrawWindowDevicePixelRatioRef = useRef<number | undefined>(
		undefined,
	);
	const initDraw = useCallback(
		async (params: FixedContentInitDrawParams) => {
			initDrawElementsRef.current = params.drawElements;
			initDrawWindowDevicePixelRatioRef.current = params.windowDevicePixelRatio;
			ocrResultActionRef.current?.setEnable(false);

			setFixedContentType(FixedContentType.DrawCanvas);

			const { canvas, captureBoundingBoxInfo, selectRectParams } = params;

			if (selectRectParams.shadowWidth > 0) {
				setShowBorder(false);
			}
			selectRectParamsRef.current = selectRectParams;

			const ocrRect = {
				min_x: 0,
				min_y: 0,
				max_x: canvas.width,
				max_y: canvas.height,
			};
			if (
				!(
					isReady?.(PLUGIN_ID_RAPID_OCR) &&
					getAppSettings()[AppSettingsGroup.FunctionFixedContent].autoOcr
				) &&
				!params.ocrResult
			) {
				initOcrParams.current = {
					selectRect: ocrRect,
					captureBoundingBoxInfo,
					canvas,
					ocrResult: undefined,
				};
			}

			const scaleFactor = await getCurrentWindow().scaleFactor();
			setWindowSize({
				width: canvas.width / scaleFactor,
				height: canvas.height / scaleFactor,
			});
			canvasPropsRef.current = {
				width: canvas.width,
				height: canvas.height,
				scaleFactor: scaleFactor,
			};

			if (selectRectParams.radius > 0) {
				setBorderRadius(selectRectParams.radius / scaleFactor);
			}

			canvasElementRef.current = canvas;
			tryInitImageLayer();
			if (ocrResultActionRef.current) {
				if (params.ocrResult) {
					// 原有的 OCR 结果不包含阴影，加个偏移
					if (selectRectParams.shadowWidth > 0) {
						params.ocrResult.result.text_blocks.forEach((textBlock) => {
							textBlock.box_points.forEach((point) => {
								point.x += selectRectParams.shadowWidth;
								point.y += selectRectParams.shadowWidth;
							});
						});
					}

					ocrResultActionRef.current.init({
						selectRect: {
							min_x: 0,
							min_y: 0,
							max_x: canvas.width,
							max_y: canvas.height,
						},
						captureBoundingBoxInfo,
						canvas,
						ocrResult: params.ocrResult,
					});
					setEnableSelectText(true);
					ocrResultActionRef.current.setEnable(true);
				} else if (
					isReady?.(PLUGIN_ID_RAPID_OCR) &&
					getAppSettings()[AppSettingsGroup.FunctionFixedContent].autoOcr
				) {
					ocrResultActionRef.current?.init({
						selectRect: {
							min_x: 0,
							min_y: 0,
							max_x: canvas.width,
							max_y: canvas.height,
						},
						captureBoundingBoxInfo,
						canvas,
						ocrResult: undefined,
					});
				}
			}

			onDrawLoad?.();
		},
		[
			setEnableSelectText,
			setFixedContentType,
			setWindowSize,
			isReady,
			onDrawLoad,
			tryInitImageLayer,
			getAppSettings,
		],
	);

	useEffect(() => {
		if (ocrResultActionRef.current) {
			ocrResultActionRef.current.setEnable(false);
		}
	}, []);

	useEffect(() => {
		if (isAlwaysOnTop) {
			appWindowRef.current?.setAlwaysOnTop(true);
			setCurrentWindowAlwaysOnTop(true);
		} else {
			appWindowRef.current?.setAlwaysOnTop(false);
		}
	}, [isAlwaysOnTop]);

	useImperativeHandle(
		actionRef,
		() => ({
			init: async (params) => {
				if ("htmlContent" in params) {
					initHtml(params.htmlContent);
				} else if ("textContent" in params) {
					initText(params.textContent);
				} else if ("canvas" in params) {
					await initDraw(params);
				} else if ("imageContent" in params) {
					initImage(params.imageContent);
				}
			},
		}),
		[initDraw, initHtml, initImage, initText],
	);

	const [scrollAction, setscrollAction, scrollActionRef] =
		useStateRef<FixedContentScrollAction>(FixedContentScrollAction.Zoom);
	const [rotateAngles, setRotateAngles, rotateAnglesRef] = useStateRef({
		x: 0,
		y: 0,
		z: 0,
	});

	const [isThumbnail, setIsThumbnail, isThumbnailRef] = useStateRef(false);
	const originWindowSizeAndPositionRef = useRef<
		| {
				size: PhysicalSize;
				position: PhysicalPosition;
				scale: {
					x: number;
					y: number;
				};
		  }
		| undefined
	>(undefined);

	const switchThumbnailAnimationRef = useRef<
		| TweenAnimation<{
				width: number;
				height: number;
				x: number;
				y: number;
		  }>
		| undefined
	>(undefined); // 切换缩略图的动画

	const switchThumbnailCore = useCallback(async () => {
		if (enableDrawRef.current) {
			return;
		}

		if (!switchThumbnailAnimationRef.current) {
			switchThumbnailAnimationRef.current = new TweenAnimation<{
				width: number;
				height: number;
				x: number;
				y: number;
			}>(
				{
					width: 0,
					height: 0,
					x: 0,
					y: 0,
				},
				TWEEN.Easing.Quadratic.Out,
				128,
				({ width, height, x, y }) => {
					const appWindow = appWindowRef.current;
					if (!appWindow) {
						return;
					}

					appWindow.setSize(
						new PhysicalSize(Math.round(width), Math.round(height)),
					);
					appWindow.setPosition(
						new PhysicalPosition(Math.round(x), Math.round(y)),
					);

					// 切换缩略图时，不会触发 mouse up 事件，这里清除下
					dragRegionMouseDownMousePositionRef.current = undefined;
				},
			);
		}

		if (!switchThumbnailAnimationRef.current.isDone()) {
			return;
		}

		const appWindow = appWindowRef.current;
		if (!appWindow) {
			return;
		}

		setDrawWindowStyle();

		if (originWindowSizeAndPositionRef.current) {
			switchThumbnailAnimationRef.current.update({
				width: originWindowSizeAndPositionRef.current.size.width,
				height: originWindowSizeAndPositionRef.current.size.height,
				x: originWindowSizeAndPositionRef.current.position.x,
				y: originWindowSizeAndPositionRef.current.position.y,
			});
			setScale({
				x: originWindowSizeAndPositionRef.current.scale.x,
				y: originWindowSizeAndPositionRef.current.scale.y,
			});
			originWindowSizeAndPositionRef.current = undefined;
			setIsThumbnail(false);
		} else {
			const [windowSize, windowPosition] = await Promise.all([
				appWindow.innerSize(),
				appWindow.outerPosition(),
			]);

			switchThumbnailAnimationRef.current.update(
				{
					width: windowSize.width,
					height: windowSize.height,
					x: windowPosition.x,
					y: windowPosition.y,
				},
				true,
			);

			originWindowSizeAndPositionRef.current = {
				size: windowSize,
				position: windowPosition,
				scale: {
					x: scaleRef.current.x,
					y: scaleRef.current.y,
				},
			};

			const thumbnailSize = Math.floor(42 * window.devicePixelRatio);

			// 获取当前鼠标位置
			const [mouseX, mouseY] = await getMousePosition();

			// 计算缩略图窗口的新位置，使其以鼠标为中心
			const newX = Math.round(mouseX - thumbnailSize / 2);
			const newY = Math.round(mouseY - thumbnailSize / 2);

			// 同时设置窗口大小和位置
			switchThumbnailAnimationRef.current.update({
				width: thumbnailSize,
				height: thumbnailSize,
				x: newX,
				y: newY,
			});

			setScale({
				x: Math.round(
					(thumbnailSize / (windowSize.width / (scaleRef.current.x / 100))) *
						100,
				),
				y: Math.round(
					(thumbnailSize / (windowSize.height / (scaleRef.current.y / 100))) *
						100,
				),
			});

			setIsThumbnail(true);
		}
	}, [enableDrawRef, scaleRef, setIsThumbnail, setScale]);
	const switchThumbnailLockRef = useRef<boolean>(false);
	const switchThumbnail = useCallback(async () => {
		if (switchThumbnailLockRef.current) {
			return;
		}
		switchThumbnailLockRef.current = true;
		await switchThumbnailCore();
		switchThumbnailLockRef.current = false;
	}, [switchThumbnailCore]);

	const [showOpacityInfo, showOpacityInfoTemporary] = useTempInfo();
	const changeContentOpacity = useCallback(
		(opacity: number) => {
			setContentOpacity(Math.min(Math.max(opacity, 0.1), 1));
			showOpacityInfoTemporary();
		},
		[setContentOpacity, showOpacityInfoTemporary],
	);

	const getWindowPhysicalSize = useCallback(
		(targetScale: number) => {
			const newWidth = Math.round(
				((canvasPropsRef.current.width * targetScale) / 100) *
					(window.devicePixelRatio /
						(canvasPropsRef.current.scaleFactor *
							(canvasPropsRef.current.ignoreTextScaleFactor
								? 1
								: textScaleFactorRef.current))),
			);
			const newHeight = Math.round(
				((canvasPropsRef.current.height * targetScale) / 100) *
					(window.devicePixelRatio /
						(canvasPropsRef.current.scaleFactor *
							(canvasPropsRef.current.ignoreTextScaleFactor
								? 1
								: textScaleFactorRef.current))),
			);

			return {
				width: newWidth,
				height: newHeight,
			};
		},
		[textScaleFactorRef],
	);

	const copyToClipboard = useCallback(async () => {
		const canvasElement = await renderToCanvas();
		if (!canvasElement) {
			return;
		}

		await copyToClipboardDrawAction(canvasElement, undefined, undefined);
	}, [renderToCanvas]);

	const saveToFile = useCallback(async () => {
		const filePath = await dialog.save({
			filters: [
				{
					name: "PNG(*.png)",
					extensions: ["png"],
				},
			],
			defaultPath: generateImageFileName(
				getAppSettings()[AppSettingsGroup.FunctionOutput]
					.manualSaveFileNameFormat,
			),
			canCreateDirectories: true,
		});

		if (!filePath) {
			return;
		}

		const canvasBlob = await renderToBlob();
		if (!canvasBlob) {
			return;
		}

		await saveFile(filePath, await canvasBlob.arrayBuffer(), ImageFormat.PNG);
	}, [getAppSettings, renderToBlob]);

	const switchSelectTextCore = useCallback(async () => {
		if (getSelectTextMode(fixedContentTypeRef.current) === "ocr") {
			if (initOcrParams.current) {
				ocrResultActionRef.current?.init(initOcrParams.current);
				initOcrParams.current = undefined;
			} else if (
				fixedContentTypeRef.current === FixedContentType.Image &&
				imageRef.current &&
				!imageOcrSignRef.current
			) {
				ocrResultActionRef.current?.init({
					imageElement: imageRef.current,
					monitorScaleFactor: window.devicePixelRatio,
				});
				imageOcrSignRef.current = true;
			}

			ocrResultActionRef.current?.setEnable((enable) => !enable);
		}

		setEnableSelectText((enable) => !enable);
	}, [fixedContentTypeRef, setEnableSelectText]);
	const switchDrawCore = useCallback(async () => {
		setEnableDraw((enable) => !enable);
	}, [setEnableDraw]);

	const switchSelectText = useCallback(async () => {
		// 启用绘制时则切换绘制
		if (enableDrawRef.current) {
			switchDrawCore();
		}

		switchSelectTextCore();
	}, [enableDrawRef, switchSelectTextCore, switchDrawCore]);
	const switchDraw = useCallback(async () => {
		if (isThumbnailRef.current) {
			return;
		}

		// 启用选择文本时则切换选择文本
		if (enableSelectTextRef.current) {
			switchSelectTextCore();
		}

		switchDrawCore();
	}, [
		enableSelectTextRef,
		isThumbnailRef,
		switchSelectTextCore,
		switchDrawCore,
	]);

	const switchAlwaysOnTop = useCallback(async () => {
		setIsAlwaysOnTop((isAlwaysOnTop) => !isAlwaysOnTop);
	}, [setIsAlwaysOnTop]);

	const [showScaleInfo, showScaleInfoTemporary] = useTempInfo();

	const scaleWindow = useCallback(
		async (scaleDelta: number, ignoreMouse: boolean = false) => {
			if (enableDrawRef.current) {
				return;
			}

			const appWindow = appWindowRef.current;
			if (!appWindow) {
				return;
			}

			if (!windowSizeRef.current.width) {
				return;
			}

			if (originWindowSizeAndPositionRef.current) {
				switchThumbnail();
				return;
			}

			const zoomWithMouse =
				getAppSettings()[AppSettingsGroup.FunctionFixedContent].zoomWithMouse;

			let targetScale = scaleRef.current.x + scaleDelta;

			if (targetScale <= SCALE_WINDOW_MIN_SCALE) {
				targetScale = SCALE_WINDOW_MIN_SCALE;
			} else if (targetScale >= SCALE_WINDOW_MAX_SCALE) {
				targetScale = SCALE_WINDOW_MAX_SCALE;
			}

			if (targetScale === scaleRef.current.x) {
				return;
			}

			setDrawWindowStyle();

			// 计算新的窗口尺寸
			const { width: newWidth, height: newHeight } =
				getWindowPhysicalSize(targetScale);

			if (zoomWithMouse && !ignoreMouse) {
				try {
					// 获取当前鼠标位置和窗口位置
					const [[mouseX, mouseY], currentPosition, currentSize] =
						await Promise.all([
							getMousePosition(),
							appWindow.outerPosition(),
							appWindow.outerSize(),
						]);

					// 计算鼠标相对于窗口的位置（比例）
					const mouseRelativeX =
						(mouseX - currentPosition.x) / currentSize.width;
					const mouseRelativeY =
						(mouseY - currentPosition.y) / currentSize.height;

					// 计算缩放后窗口的新位置，使鼠标在窗口中的相对位置保持不变
					const newX = Math.round(mouseX - newWidth * mouseRelativeX);
					const newY = Math.round(mouseY - newHeight * mouseRelativeY);

					// 同时设置窗口大小和位置
					await Promise.all([
						appWindow.setSize(new PhysicalSize(newWidth, newHeight)),
						appWindow.setPosition(new PhysicalPosition(newX, newY)),
					]);
				} catch (error) {
					appError("[scaleWindow] Error during mouse-centered scaling", error);
					// 如果出错，回退到普通缩放
					await Promise.all([
						appWindow.setSize(new PhysicalSize(newWidth, newHeight)),
					]);
				}
			} else {
				// 普通缩放，只改变窗口大小
				await Promise.all([
					appWindow.setSize(new PhysicalSize(newWidth, newHeight)),
				]);
			}

			setScale({
				x: targetScale,
				y: targetScale,
			});
			ocrResultActionRef.current?.setScale(targetScale);
			showScaleInfoTemporary();
		},
		[
			enableDrawRef,
			getAppSettings,
			getWindowPhysicalSize,
			scaleRef,
			setScale,
			showScaleInfoTemporary,
			switchThumbnail,
			windowSizeRef,
		],
	);
	const scaleWindowRender = useCallbackRender(scaleWindow);

	const rotateImage = useCallback(
		async (angle: number) => {
			// 将角度映射到 0 到 3 之间
			let currentAngle = (processImageConfigRef.current.angle + angle) % 4;
			if (currentAngle < 0) {
				currentAngle += 4;
			}
			setProcessImageConfig((prev) => ({
				...prev,
				angle: currentAngle,
			}));

			// 获取旋转前窗口的位置和大小，用于计算中心点
			const appWindow = appWindowRef.current;
			if (!appWindow) return;

			const [oldWindowSize, oldWindowPosition] = await Promise.all([
				appWindow.outerSize(),
				appWindow.outerPosition(),
			]);

			// 计算旋转前窗口的中心点
			const centerX = oldWindowPosition.x + oldWindowSize.width / 2;
			const centerY = oldWindowPosition.y + oldWindowSize.height / 2;

			// 如果角度为 1 或 3，则需要交换宽高，刚好和上一次旋转的结果相反
			setWindowSize({
				width: windowSizeRef.current.height,
				height: windowSizeRef.current.width,
			});
			canvasPropsRef.current = {
				...canvasPropsRef.current,
				width: canvasPropsRef.current.height,
				height: canvasPropsRef.current.width,
			};
			const currentWindowSize = getWindowPhysicalSize(scaleRef.current.x);

			// 先设置新的窗口大小
			await appWindow.setSize(
				new PhysicalSize(currentWindowSize.width, currentWindowSize.height),
			);

			// 根据新的窗口大小和原中心点，计算新的窗口位置
			const newX = Math.round(centerX - currentWindowSize.width / 2);
			const newY = Math.round(centerY - currentWindowSize.height / 2);

			// 设置新的窗口位置，保持中心点不变
			await appWindow.setPosition(new PhysicalPosition(newX, newY));
		},
		[
			getWindowPhysicalSize,
			processImageConfigRef,
			scaleRef,
			setProcessImageConfig,
			setWindowSize,
			windowSizeRef,
		],
	);

	const initMenuCore = useCallback(async () => {
		if (!isReadyStatus) {
			return;
		}

		if (disabled) {
			return;
		}

		const appWindow = appWindowRef.current;
		if (!appWindow) {
			return;
		}

		const menuId = `${appWindow.label}-rightClickMenu`;

		await closeRightClickMenu();
		const menu = await Menu.new({
			id: menuId,
			items: [
				{
					id: `${appWindow.label}-copyTool`,
					text: intl.formatMessage({ id: "draw.copyTool" }),
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentCopyToClipboard]?.hotKey,
					),
					action: copyToClipboard,
				},
				{
					id: `${appWindow.label}-copyRawContentTool`,
					text: intl.formatMessage({ id: "draw.copyRawContent" }),
					action: copyRawToClipboard,
				},
				{
					id: `${appWindow.label}-saveTool`,
					text: intl.formatMessage({ id: "draw.saveTool" }),
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentSaveToFile]?.hotKey,
					),
					action: saveToFile,
				},
				isReadyStatus(PLUGIN_ID_RAPID_OCR) ||
				getSelectTextMode(fixedContentType) !== "ocr"
					? {
							id: `${appWindow.label}-ocrTool`,
							text:
								getSelectTextMode(fixedContentType) === "ocr"
									? intl.formatMessage({ id: "draw.showOrHideOcrResult" })
									: intl.formatMessage({ id: "draw.selectText" }),
							accelerator: formatKey(
								hotkeys?.[CommonKeyEventKey.FixedContentSelectText]?.hotKey,
							),
							checked: enableSelectText,
							action: switchSelectText,
						}
					: undefined,
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-enableDrawTool`,
					text: intl.formatMessage({
						id: "settings.hotKeySettings.fixedContent.fixedContentEnableDraw",
					}),
					checked: enableDraw,
					enabled: !isThumbnail,
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentEnableDraw]?.hotKey,
					),
					action: switchDraw,
				},
				{
					id: `${appWindow.label}-processImageTool`,
					text: intl.formatMessage({ id: "draw.processImage" }),
					items: [
						{
							id: `${appWindow.label}-processImageToolRotateLeft`,
							text: `${intl.formatMessage({ id: "draw.processImage.rotateLeft" })} 🔄`,
							action: () => {
								rotateImage(-1);
							},
						},
						{
							id: `${appWindow.label}-processImageToolRotateRight`,
							text: `${intl.formatMessage({ id: "draw.processImage.rotateRight" })} 🔃`,
							action: () => {
								rotateImage(1);
							},
						},
						{
							id: `${appWindow.label}-processImageToolHorizontalFlip`,
							text: `${intl.formatMessage({ id: "draw.processImage.horizontalFlip" })} ↔️`,
							action: () => {
								setProcessImageConfig((prev) => ({
									...prev,
									horizontalFlip: !prev.horizontalFlip,
								}));
							},
						},
						{
							id: `${appWindow.label}-processImageToolVerticalFlip`,
							text: `${intl.formatMessage({ id: "draw.processImage.verticalFlip" })} ↕️`,
							action: () => {
								setProcessImageConfig((prev) => ({
									...prev,
									verticalFlip: !prev.verticalFlip,
								}));
							},
						},
					],
				},
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-switchThumbnailTool`,
					text: intl.formatMessage({ id: "draw.switchThumbnail" }),
					checked: isThumbnail,
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentSwitchThumbnail]?.hotKey,
					),
					enabled: !enableDraw,
					action: async () => {
						switchThumbnail();
					},
				},
				await Submenu.new({
					id: `${appWindow.label}-focusModeTool`,
					text: intl.formatMessage({ id: "draw.focusMode" }),
					items: [
						{
							id: `${appWindow.label}-focusModeToolShowAllWindow`,
							text: intl.formatMessage({ id: "draw.focusMode.showAllWindow" }),
							action: fixedContentFocusModeShowAllWindow,
						},
						{
							id: `${appWindow.label}-focusModeToolHideOtherWindow`,
							text: intl.formatMessage({
								id: "draw.focusMode.hideOtherWindow",
							}),
							action: fixedContentFocusModeHideOtherWindow,
						},
						{
							id: `${appWindow.label}-focusModeToolCloseOtherWindow`,
							text: intl.formatMessage({
								id: "draw.focusMode.closeOtherWindow",
							}),
							action: fixedContentFocusModeCloseOtherWindow,
						},
						{
							id: `${appWindow.label}-focusModeToolCloseAllWindow`,
							text: intl.formatMessage({ id: "draw.focusMode.closeAllWindow" }),
							action: fixedContentFocusModeCloseAllWindow,
						},
					],
				}),
				{
					id: `${appWindow.label}-switchAlwaysOnTopTool`,
					text: intl.formatMessage({
						id: "settings.hotKeySettings.fixedContent.fixedContentAlwaysOnTop",
					}),
					checked: isAlwaysOnTop,
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentAlwaysOnTop]?.hotKey,
					),
					action: switchAlwaysOnTop,
				},
				{
					item: "Separator",
				},
				await Submenu.new({
					id: `${appWindow.label}-setOpacityTool`,
					text: intl.formatMessage({
						id: "settings.hotKeySettings.fixedContent.opacity",
					}),
					items: [
						{
							id: `${appWindow.label}-setOpacityTool25`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setOpacity.twentyFive",
							}),
							action: () => {
								changeContentOpacity(0.25);
							},
						},
						{
							id: `${appWindow.label}-setOpacityTool50`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setOpacity.fifty",
							}),
							action: () => {
								changeContentOpacity(0.5);
							},
						},
						{
							id: `${appWindow.label}-setOpacityTool75`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setOpacity.seventyFive",
							}),
							action: () => {
								changeContentOpacity(0.75);
							},
						},
						{
							id: `${appWindow.label}-setOpacityTool100`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setOpacity.hundred",
							}),
							action: () => {
								changeContentOpacity(1);
							},
						},
					],
				}),
				await Submenu.new({
					id: `${appWindow.label}-setScaleTool`,
					text: intl.formatMessage({
						id: "settings.hotKeySettings.fixedContent.scale",
					}),
					enabled: !enableDraw,
					items: [
						{
							id: `${appWindow.label}-setScaleTool25`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setScale.twentyFive",
							}),
							action: () => {
								scaleWindow(25 - scaleRef.current.x, true);
							},
						},
						{
							id: `${appWindow.label}-setScaleTool50`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setScale.fifty",
							}),
							action: () => {
								scaleWindow(50 - scaleRef.current.x, true);
							},
						},
						{
							id: `${appWindow.label}-setScaleTool75`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setScale.seventyFive",
							}),
							action: () => {
								scaleWindow(75 - scaleRef.current.x, true);
							},
						},
						{
							id: `${appWindow.label}-setScaleTool100`,
							text: intl.formatMessage({
								id: "settings.hotKeySettings.fixedContent.setScale.hundred",
							}),
							action: () => {
								scaleWindow(100 - scaleRef.current.x, true);
							},
						},
					],
				}),
				{
					id: `${appWindow.label}-scrollActionTool`,
					text: intl.formatMessage({ id: "draw.scrollAction" }),
					enabled: !enableDraw,
					items: [
						{
							id: `${appWindow.label}-scrollActionToolZoom`,
							text: intl.formatMessage({ id: "draw.scrollAction.zoom" }),
							checked: scrollAction === FixedContentScrollAction.Zoom,
							action: () => {
								setscrollAction(FixedContentScrollAction.Zoom);
							},
						},
						{
							id: `${appWindow.label}-scrollActionToolRotateX`,
							text: intl.formatMessage({ id: "draw.scrollAction.rotateX" }),
							checked: scrollAction === FixedContentScrollAction.RotateX,
							action: () => {
								setscrollAction(FixedContentScrollAction.RotateX);
							},
						},
						{
							id: `${appWindow.label}-scrollActionToolRotateY`,
							text: intl.formatMessage({ id: "draw.scrollAction.rotateY" }),
							checked: scrollAction === FixedContentScrollAction.RotateY,
							action: () => {
								setscrollAction(FixedContentScrollAction.RotateY);
							},
						},
						{
							id: `${appWindow.label}-scrollActionToolRotateZ`,
							text: intl.formatMessage({ id: "draw.scrollAction.rotateZ" }),
							checked: scrollAction === FixedContentScrollAction.RotateZ,
							action: () => {
								setscrollAction(FixedContentScrollAction.RotateZ);
							},
						},
					],
				},
				{
					item: "Separator",
				},
				{
					id: `${appWindow.label}-closeTool`,
					text: intl.formatMessage({ id: "draw.close" }),
					accelerator: formatKey(
						hotkeys?.[CommonKeyEventKey.FixedContentCloseWindow]?.hotKey,
					),
					action: async () => {
						await closeWindowComplete();
					},
				},
			].filter((item) => item !== undefined) as MenuItemOptions[],
		});
		rightClickMenu = menu;
	}, [
		isReadyStatus,
		disabled,
		intl,
		hotkeys,
		copyToClipboard,
		copyRawToClipboard,
		saveToFile,
		fixedContentType,
		enableSelectText,
		switchSelectText,
		enableDraw,
		switchDraw,
		isThumbnail,
		isAlwaysOnTop,
		switchAlwaysOnTop,
		scrollAction,
		rotateImage,
		setProcessImageConfig,
		switchThumbnail,
		changeContentOpacity,
		scaleWindow,
		scaleRef,
		setscrollAction,
	]);
	const initMenu = useCallbackRender(initMenuCore);

	useEffect(() => {
		initMenu();

		return () => {
			closeRightClickMenu();
		};
	}, [initMenu]);

	const onWheel = useCallback(
		(event: React.WheelEvent<HTMLDivElement>) => {
			if (enableDrawRef.current) {
				return;
			}

			const { deltaY } = event;

			if (
				isHotkeyPressed(
					hotkeys?.[CommonKeyEventKey.FixedContentSetOpacity]?.hotKey ?? "",
				)
			) {
				if (deltaY > 0) {
					changeContentOpacity(contentOpacityRef.current - 0.05);
				} else {
					changeContentOpacity(contentOpacityRef.current + 0.05);
				}
				return;
			}

			const delta = deltaY > 0 ? -1 : 1;

			if (scrollActionRef.current === FixedContentScrollAction.Zoom) {
				scaleWindowRender(delta * 10);
			} else if (scrollActionRef.current === FixedContentScrollAction.RotateX) {
				setRotateAngles({
					...rotateAnglesRef.current,
					x: rotateAnglesRef.current.x + delta * 3,
				});
			} else if (scrollActionRef.current === FixedContentScrollAction.RotateY) {
				setRotateAngles({
					...rotateAnglesRef.current,
					y: rotateAnglesRef.current.y + delta * 3,
				});
			} else if (scrollActionRef.current === FixedContentScrollAction.RotateZ) {
				setRotateAngles({
					...rotateAnglesRef.current,
					z: rotateAnglesRef.current.z + delta * 3,
				});
			}
		},
		[
			changeContentOpacity,
			contentOpacityRef,
			enableDrawRef,
			hotkeys,
			rotateAnglesRef,
			scaleWindowRender,
			scrollActionRef,
			setRotateAngles,
		],
	);

	const onScrollDown = useCallback(
		(e: React.MouseEvent<HTMLDivElement>) => {
			if (e.button !== 1) {
				return;
			}

			scaleWindow(100 - scaleRef.current.x, false);
		},
		[scaleRef, scaleWindow],
	);

	const handleContextMenu = useCallback(
		async (e: React.MouseEvent<HTMLDivElement>) => {
			e.preventDefault();
			e.stopPropagation();

			await rightClickMenu?.popup();
		},
		[],
	);

	useEffect(() => {
		if (disabled) {
			return;
		}

		if (fixedContentType !== FixedContentType.Html) {
			return;
		}

		const handleMessage = (event: MessageEvent) => {
			const { type, x, y, width, height, href } = event.data;

			if (
				(type === "bodySize" || type === "resize") &&
				htmlContentContainerRef.current &&
				canvasPropsRef.current.width === 0
			) {
				if (width === 800 && type !== "resize") {
					if (htmlContentContainerRef.current) {
						htmlContentContainerRef.current.style.width = `${1000}px`;
					}
					return;
				}

				if (htmlContentContainerRef.current) {
					htmlContentContainerRef.current.style.width = `${width}px`;
					htmlContentContainerRef.current.style.height = `${height}px`;
				}
				onHtmlLoad?.({
					width: width * window.devicePixelRatio,
					height: height * window.devicePixelRatio,
				});

				setWindowSize({
					width: width,
					height: height,
				});
				canvasPropsRef.current = {
					width: width * window.devicePixelRatio,
					height: height * window.devicePixelRatio,
					scaleFactor: window.devicePixelRatio,
					ignoreTextScaleFactor: true,
				};

				tryInitImageLayer(true);
			} else if (type === "contextMenu") {
				// 处理来自iframe的右键菜单事件
				const syntheticEvent = {
					preventDefault: () => {},
					stopPropagation: () => {},
					clientX: x,
					clientY: y,
				} as React.MouseEvent<HTMLDivElement>;
				handleContextMenu(syntheticEvent);
			} else if (type === "wheel") {
				onWheel(
					event.data.eventData as unknown as React.WheelEvent<HTMLDivElement>,
				);
			} else if (type === "linkClick") {
				openUrl(href);
			} else if (type === "keydown" || type === "keyup") {
				// 创建并触发自定义键盘事件
				const keyEvent = new KeyboardEvent(type, {
					key: event.data.key,
					code: event.data.code,
					keyCode: event.data.keyCode,
					ctrlKey: event.data.ctrlKey,
					shiftKey: event.data.shiftKey,
					altKey: event.data.altKey,
					metaKey: event.data.metaKey,
					repeat: event.data.repeat,
					bubbles: true,
					cancelable: true,
				});
				document.dispatchEvent(keyEvent);
			} else if (type === "mousedown") {
				onScrollDown(event.data as unknown as React.MouseEvent<HTMLDivElement>);
			}
		};

		window.addEventListener("message", handleMessage);

		return () => {
			window.removeEventListener("message", handleMessage);
		};
	}, [
		onHtmlLoad,
		setWindowSize,
		handleContextMenu,
		onWheel,
		disabled,
		fixedContentType,
		onScrollDown,
		tryInitImageLayer,
	]);

	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentSwitchThumbnail]?.hotKey ?? "",
		switchThumbnail,
		useMemo(
			() => ({
				keyup: true,
				keydown: false,
				enabled: !disabled,
				preventDefault: true,
			}),
			[disabled],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentCloseWindow]?.hotKey ?? "",
		closeWindowComplete,
		useMemo(
			() => ({
				keyup: false,
				keydown: true,
				enabled: !disabled,
				preventDefault: true,
			}),
			[disabled],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentCopyToClipboard]?.hotKey ?? "",
		copyToClipboard,
		useMemo(
			() => ({
				keyup: false,
				keydown: true,
				enabled: !disabled && !enableSelectText,
				preventDefault: true,
			}),
			[disabled, enableSelectText],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentEnableDraw]?.hotKey ?? "",
		switchDraw,
		useMemo(
			() => ({
				keyup: true,
				keydown: false,
				enabled: !disabled && !enableSelectText,
				preventDefault: true,
			}),
			[disabled, enableSelectText],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentSelectText]?.hotKey ?? "",
		switchSelectText,
		useMemo(
			() => ({
				keyup: false,
				keydown: true,
				enabled: !disabled,
				preventDefault: true,
			}),
			[disabled],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentSaveToFile]?.hotKey ?? "",
		saveToFile,
		useMemo(
			() => ({
				keyup: false,
				keydown: true,
				enabled: !disabled,
				preventDefault: true,
			}),
			[disabled],
		),
	);
	useHotkeys(
		hotkeys?.[CommonKeyEventKey.FixedContentAlwaysOnTop]?.hotKey ?? "",
		switchAlwaysOnTop,
		useMemo(
			() => ({
				keyup: false,
				keydown: true,
				enabled: !disabled,
				preventDefault: true,
			}),
			[disabled],
		),
	);

	const onDoubleClick = useCallback(
		(e: React.MouseEvent<HTMLDivElement>) => {
			e.stopPropagation();
			e.preventDefault();
			switchThumbnail();
		},
		[switchThumbnail],
	);

	const onDragRegionMouseDown = useCallback(
		(e: React.MouseEvent<HTMLDivElement>) => {
			if (e.button !== 0) {
				onScrollDown(e);
				return;
			}

			e.stopPropagation();
			e.preventDefault();

			if (enableDrawRef.current) {
				return;
			}

			dragRegionMouseDownMousePositionRef.current = undefined;

			if (e.button === 0) {
				dragRegionMouseDownMousePositionRef.current = new MousePosition(
					e.clientX,
					e.clientY,
				);
			}
		},
		[enableDrawRef, onScrollDown],
	);
	const onDragRegionMouseMove = useCallback(
		(e: React.MouseEvent<HTMLDivElement>) => {
			e.stopPropagation();
			e.preventDefault();

			if (!dragRegionMouseDownMousePositionRef.current) {
				return;
			}

			const distance = dragRegionMouseDownMousePositionRef.current.getDistance(
				new MousePosition(e.clientX, e.clientY),
			);
			// 缩略模式降低拖拽阈值
			if (distance > 6 || (isThumbnailRef.current && distance > 2)) {
				dragRegionMouseDownMousePositionRef.current = undefined;
				startFreeDrag().catch((error) => {
					appError("[FixedContentCore] startFreeDrag error", error);
					message.error(<FormattedMessage id="draw.captureAllMonitorsError" />);
				});
			}
		},
		[isThumbnailRef, message],
	);
	const onDragRegionMouseUp = useCallback(
		(e: React.MouseEvent<HTMLDivElement>) => {
			e.stopPropagation();
			e.preventDefault();

			dragRegionMouseDownMousePositionRef.current = undefined;
		},
		[],
	);

	const updateDrawWindowSize = useCallback(async () => {
		if (!appWindowRef.current || !drawActionRef.current) {
			return;
		}

		const currentWindowSize = getWindowPhysicalSize(scale.x);
		const targetWindowSize = {
			...currentWindowSize,
		};

		const toolbarSize = drawActionRef.current.getToolbarSize();
		toolbarSize.width = Math.ceil(toolbarSize.width * window.devicePixelRatio);
		toolbarSize.height = Math.ceil(
			toolbarSize.height * window.devicePixelRatio,
		);

		const drawMenuSize = drawActionRef.current.getDrawMenuSize();
		drawMenuSize.width = Math.ceil(
			drawMenuSize.width * window.devicePixelRatio,
		);
		drawMenuSize.height = Math.ceil(
			drawMenuSize.height * window.devicePixelRatio,
		);

		const minHeight = Math.max(
			currentWindowSize.height + toolbarSize.height,
			drawMenuSize.height,
		);
		const minWidth = Math.max(
			drawMenuSize.width + currentWindowSize.width,
			toolbarSize.width,
		);

		if (enableDraw) {
			targetWindowSize.height = minHeight;
			targetWindowSize.width = minWidth;
		}
		appWindowRef.current.setSize(
			new PhysicalSize(targetWindowSize.width, targetWindowSize.height),
		);

		if (enableDraw) {
			setEnableDrawLayer(true);
		} else {
			setEnableDrawLayer(false);
		}
	}, [enableDraw, getWindowPhysicalSize, scale.x]);
	useEffect(() => {
		updateDrawWindowSize();
	}, [updateDrawWindowSize]);

	const documentSize = useMemo<FixedContentWindowSize>(() => {
		return {
			width: ((windowSize.width / contentScaleFactor) * scale.x) / 100,
			height: ((windowSize.height / contentScaleFactor) * scale.y) / 100,
		};
	}, [
		contentScaleFactor,
		scale.x,
		scale.y,
		windowSize.height,
		windowSize.width,
	]);

	const getAspectRatio = useCallback(() => {
		return windowSizeRef.current.height / windowSizeRef.current.width;
	}, [windowSizeRef]);
	const getMinWidth = useCallback(() => {
		const windowSize = getWindowPhysicalSize(SCALE_WINDOW_MIN_SCALE);

		return windowSize.width;
	}, [getWindowPhysicalSize]);
	const getMaxWidth = useCallback(() => {
		const windowSize = getWindowPhysicalSize(SCALE_WINDOW_MAX_SCALE);

		return windowSize.width;
	}, [getWindowPhysicalSize]);

	const onResize = useCallback(
		(size: { width: number; height: number }) => {
			const windowSize = getWindowPhysicalSize(100);

			const scale = size.width / windowSize.width;
			const targetScale = scale * 100;
			setScale({
				x: targetScale,
				y: targetScale,
			});
			ocrResultActionRef.current?.setScale(targetScale);
			showScaleInfoTemporary();
		},
		[getWindowPhysicalSize, setScale, showScaleInfoTemporary],
	);

	const imageLayerActionRef = useRef<
		FixedContentImageLayerActionType | undefined
	>(undefined);

	const onImageLayerReady = useCallback(() => {
		tryInitImageLayer();
	}, [tryInitImageLayer]);

	const getSelectRectParams = useCallback(() => {
		const currentSelectRectParams = selectRectParamsRef.current;
		if (!currentSelectRectParams) {
			return {
				rect: {
					min_x: 0,
					min_y: 0,
					max_x: canvasPropsRef.current.width,
					max_y: canvasPropsRef.current.height,
				},
				radius: 0,
				shadowWidth: 0,
				shadowColor: "#000000",
			};
		}

		const result = {
			rect: {
				min_x: currentSelectRectParams.shadowWidth,
				min_y: currentSelectRectParams.shadowWidth,
				max_x:
					canvasPropsRef.current.width - currentSelectRectParams.shadowWidth,
				max_y:
					canvasPropsRef.current.height - currentSelectRectParams.shadowWidth,
			},
			radius: currentSelectRectParams.radius,
			shadowWidth: currentSelectRectParams.shadowWidth,
			shadowColor: currentSelectRectParams.shadowColor,
		};

		return result;
	}, []);

	const getInitDrawSelectRectParams = useCallback(() => {
		return selectRectParamsRef.current;
	}, []);

	const getImageLayerAction = useCallback(() => {
		return imageLayerActionRef.current?.getImageLayerAction();
	}, []);

	const getInitDrawDrawElements = useCallback(() => {
		return initDrawElementsRef.current;
	}, []);

	const getInitDrawWindowDevicePixelRatio = useCallback(() => {
		return initDrawWindowDevicePixelRatioRef.current;
	}, []);

	const getZoom = useCallback(() => {
		return (
			scaleRef.current.x /
			100 /
			((canvasPropsRef.current.scaleFactor *
				(canvasPropsRef.current.ignoreTextScaleFactor
					? 1
					: textScaleFactorRef.current)) /
				window.devicePixelRatio)
		);
	}, [scaleRef, textScaleFactorRef]);

	return (
		<div
			className="fixed-image-container"
			style={{
				position: "absolute",
				width: `${documentSize.width}px`,
				height: `${documentSize.height}px`,
				zIndex: zIndexs.Draw_FixedImage,
				pointerEvents: disabled ? "none" : "auto",
				opacity: isThumbnail ? 0.72 : contentOpacity,
				userSelect: isThumbnail || !enableSelectText ? "none" : undefined,
			}}
			onContextMenu={handleContextMenu}
			onDoubleClick={onDoubleClick}
			onMouseDown={!enableDraw ? onDragRegionMouseDown : undefined}
			onMouseMove={!enableDraw ? onDragRegionMouseMove : undefined}
			onMouseUp={!enableDraw ? onDragRegionMouseUp : undefined}
		>
			<HandleFocusMode disabled={disabled} />

			<div className="fixed-image-container-content">
				<OcrResult
					actionRef={ocrResultActionRef}
					zIndex={1}
					onWheel={onWheel}
					onContextMenu={handleContextMenu}
					disabled={disabled || getSelectTextMode(fixedContentType) !== "ocr"}
					enableCopy
					onMouseDown={onDragRegionMouseDown}
					onMouseMove={onDragRegionMouseMove}
					onMouseUp={onDragRegionMouseUp}
					style={{
						...getStyleProps(
							(windowSize.width * scale.x) / 100 / contentScaleFactor,
							(windowSize.height * scale.y) / 100 / contentScaleFactor,
							processImageConfig,
						),
					}}
				/>

				{htmlContent && (
					<iframe
						title="fixed-html-content"
						style={{
							...getStyleProps(
								(windowSize.width * scale.x) / 100 / contentScaleFactor,
								(windowSize.height * scale.y) / 100 / contentScaleFactor,
								processImageConfig,
								{
									scale: {
										x: scale.x / 100 / contentScaleFactor,
										y: scale.y / 100 / contentScaleFactor,
									},
								},
							),
							width: undefined,
							height: undefined,
							zIndex: enableSelectText ? 1 : "unset",
							position: "absolute",
							top: 0,
							left: 0,
							right: 0,
							bottom: 0,
							backgroundColor: token.colorBgContainer,
						}}
						ref={htmlContentContainerRef}
						srcDoc={getHtmlContent(token, htmlContent)}
						className="fixed-html-content"
					/>
				)}

				{textContent && (
					<div
						style={{
							...getStyleProps(
								(windowSize.width * scale.x) / 100 / contentScaleFactor,
								(windowSize.height * scale.y) / 100 / contentScaleFactor,
								processImageConfig,
								{
									scale: {
										x: scale.x / 100 / contentScaleFactor,
										y: scale.y / 100 / contentScaleFactor,
									},
								},
							),
							width: undefined,
							height: undefined,
							zIndex: enableSelectText ? 1 : "unset",
							position: "absolute",
							top: 0,
							left: 0,
							right: 0,
							bottom: 0,
						}}
						onMouseDown={(event) => {
							event.stopPropagation();
						}}
						onMouseMove={(event) => {
							event.stopPropagation();
						}}
						onMouseUp={(event) => {
							event.stopPropagation();
						}}
					>
						<div ref={textContentContainerRef} className="fixed-text-content">
							{!textContent?.colorText && (
								<div style={{ userSelect: "text", display: "inline-block" }}>
									{textContent?.content}
								</div>
							)}
							{textContent?.colorText && (
								<div
									onMouseDown={
										!enableSelectText ? onDragRegionMouseDown : undefined
									}
									onMouseMove={
										!enableSelectText ? onDragRegionMouseMove : undefined
									}
									onMouseUp={
										!enableSelectText ? onDragRegionMouseUp : undefined
									}
									className={!enableSelectText ? "fixed-text-content-drag" : ""}
								>
									<Descriptions>
										<Descriptions.Item
											label={<FormattedMessage id="draw.color" />}
											span={1}
										>
											<div
												style={{
													display: "inline-flex",
													alignItems: "center",
													height: "100%",
												}}
											>
												<div
													style={{
														backgroundColor: textContent.colorText.color,
														width: "16px",
														height: "16px",
														borderRadius: "2px",
														boxShadow: token.boxShadowTertiary,
													}}
												/>
											</div>
										</Descriptions.Item>
										<Descriptions.Item label="HEX" span={1}>
											<Typography.Text copyable>
												{textContent.colorText.hex}
											</Typography.Text>
										</Descriptions.Item>
										<Descriptions.Item label="RGB" span={1}>
											<Typography.Text copyable>
												{textContent?.colorText.rgb}
											</Typography.Text>
										</Descriptions.Item>

										<Descriptions.Item label="HSL" span={1}>
											<Typography.Text copyable>
												{textContent.colorText.hsl}
											</Typography.Text>
										</Descriptions.Item>
									</Descriptions>
								</div>
							)}
						</div>
					</div>
				)}

				<div
					className="fixed-image-layer-container"
					style={{
						...getStyleProps(
							(windowSize.width * scale.x) / 100 / contentScaleFactor,
							(windowSize.height * scale.y) / 100 / contentScaleFactor,
							processImageConfig,
						),
					}}
				>
					<FixedContentImageLayer
						actionRef={imageLayerActionRef}
						onImageLayerReady={onImageLayerReady}
					/>
				</div>
			</div>

			{!disabled && (
				<DrawLayer
					actionRef={drawActionRef}
					documentSize={documentSize}
					scaleInfo={scale}
					disabled={!enableDraw || !enableDrawLayer}
					hidden={enableSelectText}
					onConfirm={switchDraw}
					getImageLayerAction={getImageLayerAction}
					getSelectRectParams={getSelectRectParams}
					getInitDrawDrawElements={getInitDrawDrawElements}
					getInitDrawSelectRectParams={getInitDrawSelectRectParams}
					getInitDrawWindowDevicePixelRatio={getInitDrawWindowDevicePixelRatio}
					getZoom={getZoom}
				/>
			)}

			<div
				className="fixed-image-container-inner"
				onWheel={onWheel}
				onMouseDown={onScrollDown}
			>
				<div className="fixed-image-container-inner-border"></div>
				<div className="fixed-image-container-inner-resize-window">
					<ResizeWindow
						getAspectRatio={getAspectRatio}
						getMinWidth={getMinWidth}
						getMaxWidth={getMaxWidth}
						onResize={onResize}
					/>
				</div>

				<Space
					className="fixed-image-button-group"
					style={{
						position: "absolute",
						top: token.margin,
						right: token.margin,
						opacity: 0,
						transition: `opacity ${token.motionDurationFast} ${token.motionEaseInOut}`,
						zIndex: zIndexs.FixedToScreen_CloseButton,
						// iframe 无法点击 close 按钮
						display:
							isThumbnail || enableDraw || enableSelectText
								? "none"
								: undefined,
						pointerEvents: "auto",
					}}
				>
					<Button
						icon={<EditOutlined />}
						style={{
							backgroundColor: token.colorBgMask,
							transition: `background-color ${token.motionDurationFast} ${token.motionEaseInOut}`,
						}}
						className="fixed-image-edit-button"
						type="primary"
						shape="circle"
						variant="solid"
						onClick={() => {
							switchDraw();
						}}
					/>

					<Button
						icon={<CloseOutlined />}
						style={{
							backgroundColor: token.colorBgMask,
							transition: `background-color ${token.motionDurationFast} ${token.motionEaseInOut}`,
						}}
						className="fixed-image-close-button"
						type="primary"
						shape="circle"
						variant="solid"
						onClick={() => {
							closeWindowComplete();
						}}
					/>
				</Space>

				<div className="scale-info" style={{ opacity: showScaleInfo ? 1 : 0 }}>
					<FormattedMessage
						id="settings.hotKeySettings.fixedContent.scaleInfo"
						values={{ scale: scale.x.toFixed(0) }}
					/>
				</div>

				<div
					className="scale-info"
					style={{ opacity: showOpacityInfo ? 1 : 0 }}
				>
					<FormattedMessage
						id="settings.hotKeySettings.fixedContent.opacityInfo"
						values={{ opacity: (contentOpacity * 100).toFixed(0) }}
					/>
				</div>
			</div>

			<style jsx>{`
                .fixed-image-container {
                    display: ${disabled ? "none" : "block"};
                }

                .fixed-image-container-content {
                    transformorigin: center center;
                    transform: rotateX(${rotateAngles.x}deg) rotateY(${rotateAngles.y}deg)
                        rotateZ(${rotateAngles.z}deg);
                }

                .fixed-image-container:hover :global(.fixed-image-button-group) {
                    opacity: 1 !important;
                }

                .fixed-image-container
                    :global(.fixed-image-button-group .fixed-image-edit-button):hover {
                    background-color: ${token.colorPrimary} !important;
                }

                .fixed-image-container
                    :global(.fixed-image-button-group .fixed-image-close-button):hover {
                    background-color: ${token.colorError} !important;
                }

                .fixed-image-layer-container {
                }

                .fixed-image-container-inner {
                    width: calc(${isThumbnail ? "100vw" : `${documentSize.width}px`});
                    height: calc(${isThumbnail ? "100vh" : `${documentSize.height}px`});
                    position: absolute;
                    top: 0;
                    left: 0;
                    cursor: grab;
                    box-sizing: border-box;
                    pointer-events: ${
											(enableSelectText || textContent?.colorText) &&
											!isThumbnail
												? "none"
												: "auto"
										};
                }

                .fixed-image-container-inner-border {
                    position: fixed;
                    top: 0;
                    left: 0;
                    width: calc(${isThumbnail ? "100vw" : `${documentSize.width}px`});
                    height: calc(${isThumbnail ? "100vh" : `${documentSize.height}px`});
                    border: 2px solid ${fixedBorderColor ?? token.colorBorder};
                    box-sizing: border-box;
                    pointer-events: none;
                    z-index: ${zIndexs.FixedToScreen_Border};
                    display: ${showBorder ? "block" : "none"};
                    border-radius: ${(borderRadius * (scale.x / 100)) / textScaleFactor}px;
                }

                .fixed-image-container-inner-resize-window {
                    position: fixed;
                    top: 0;
                    left: 0;
                    width: calc(${isThumbnail ? "100vw" : `${documentSize.width}px`});
                    height: calc(${isThumbnail ? "100vh" : `${documentSize.height}px`});
                    pointer-events: none;
                    z-index: ${zIndexs.FixedToScreen_ResizeWindow};
                    display: ${isThumbnail || enableDraw || enableSelectText ? "none" : "block"};
                }

                .fixed-image-container-inner:active {
                    cursor: grabbing;
                }

                .fixed-text-content-drag {
                    cursor: grab;
                }

                .fixed-text-content-drag:active {
                    cursor: grabbing;
                }

                .fixed-html-content,
                .fixed-text-content {
                    position: absolute;
                    top: 0;
                    left: 0;
                    border: unset !important;
                }

                .fixed-html-content {
                    width: 800px;
                    height: 0px;
                    user-select: none;
                }

                .fixed-text-content {
                    width: auto;
                    white-space: pre;
                    background-color: ${token.colorBgContainer};
                    color: ${token.colorText};
                    padding: ${token.padding}px;
                    box-sizing: border-box;
                }

                .fixed-text-content :global(.ant-typography-copy) {
                    z-index: ${zIndexs.FixedToScreen_CloseButton};
                }

                .fixed-html-content > :global(div):first-child {
                    padding: ${token.padding}px;
                }

                .scale-info {
                    position: absolute;
                    bottom: 0;
                    left: 0;
                    background-color: ${token.colorBgMask};
                    color: ${token.colorWhite};
                    padding: ${token.paddingXXS}px ${token.paddingSM}px;
                    border-top-right-radius: ${token.borderRadius}px;
                    font-size: ${token.fontSizeSM}px;
                    z-index: ${zIndexs.FixedToScreen_ScaleInfo};
                    transition: opacity ${token.motionDurationFast} ${token.motionEaseInOut};
                    display: ${isThumbnail || enableDraw || enableSelectText ? "none" : "block"};
                }

                /* 
                 * 窗口过小的情况下隐藏关闭按钮
                 */
                @media screen and (max-width: 160px) {
                    .fixed-image-container :global(.fixed-image-button-group) {
                        display: none !important;
                    }
                }

                @media screen and (max-height: 83px) {
                    .fixed-image-container :global(.fixed-image-button-group) {
                        display: none !important;
                    }
                }

                @media screen and (max-width: 200px) {
                    .fixed-image-container .scale-info {
                        display: none !important;
                    }
                }

                @media screen and (max-height: 128px) {
                    .fixed-image-container .scale-info {
                        display: none !important;
                    }
                }
            `}</style>
		</div>
	);
};
