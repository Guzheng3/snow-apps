import type { Window as AppWindow } from "@tauri-apps/api/window";
import { createDrawWindow, saveFile } from "@/commands";
import { writeBitmapImageToClipboard } from "@/commands/core";
import { setExcludeFromCapture } from "@/commands/videoRecord";
import { type AppSettingsData, AppSettingsGroup } from "@/types/appSettings";
import { ImageFormat, type ImagePath } from "@/types/utils/file";
import { writeImageToClipboard } from "@/utils/clipboard";
import { showImageDialog } from "@/utils/file";
import { appError, appWarn } from "@/utils/log";
import { getPlatform } from "@/utils/platform";
import { setWindowRect } from "@/utils/window";
import type { FixedContentActionType } from "../fixedContent/components/fixedContentCore";
import type { AppOcrResult } from "../fixedContent/components/ocrResult";
import type { DrawCacheLayerActionType } from "./components/drawCacheLayer/extra";
import type { DrawLayerActionType } from "./components/drawLayer";
import type { OcrBlocksActionType } from "./components/ocrBlocks";
import type {
	SelectLayerActionType,
	SelectRectParams,
} from "./components/selectLayer";
import type { CaptureBoundingBoxInfo } from "./extra";
import { CaptureStep } from "./types";

export const getCanvas = async (
	selectRectParams: SelectRectParams | undefined,
	drawLayerAction: DrawLayerActionType,
	drawCacheLayerAction: DrawCacheLayerActionType,
	ignoreStyle?: boolean,
): Promise<HTMLCanvasElement | undefined> => {
	if (!selectRectParams) {
		return;
	}

	const { rect: selectRect, shadowColor: selectRectShadowColor } =
		selectRectParams;

	let selectRectShadowWidth = 0;
	let selectRectRadius = 0;
	if (!ignoreStyle) {
		selectRectShadowWidth = selectRectParams.shadowWidth;
		selectRectRadius = selectRectParams.radius;
	}

	drawCacheLayerAction.finishDraw();

	// 获取图像数据
	const drawLayerImageData = await drawLayerAction.getImageData(selectRect);
	const drawCacheLayerCanvas = drawCacheLayerAction.getCanvas();

	if (!drawLayerImageData || !drawCacheLayerCanvas) {
		return;
	}

	const offsetX = selectRectShadowWidth;
	const offsetY = selectRectShadowWidth;

	const tempCanvas = document.createElement("canvas");

	const contentWidth = selectRect.max_x - selectRect.min_x;
	const contentHeight = selectRect.max_y - selectRect.min_y;

	tempCanvas.width = contentWidth + selectRectShadowWidth * 2;
	tempCanvas.height = contentHeight + selectRectShadowWidth * 2;
	const tempCtx = tempCanvas.getContext("2d");
	if (!tempCtx) {
		return;
	}

	tempCtx.putImageData(drawLayerImageData, offsetX, offsetY);
	tempCtx.drawImage(
		drawCacheLayerCanvas,
		-selectRect.min_x + offsetX,
		-selectRect.min_y + offsetY,
	);

	if (selectRectRadius > 0 || selectRectShadowWidth > 0) {
		tempCtx.save();

		tempCtx.beginPath();
		tempCtx.rect(0, 0, tempCanvas.width, tempCanvas.height);
		if (selectRectRadius > 0) {
			tempCtx.roundRect(
				offsetX,
				offsetY,
				contentWidth,
				contentHeight,
				selectRectRadius,
			);
		} else {
			tempCtx.rect(offsetX, offsetY, contentWidth, contentHeight);
		}
		tempCtx.clip("evenodd");
		tempCtx.clearRect(0, 0, tempCanvas.width, tempCanvas.height);

		tempCtx.restore();
	}

	if (selectRectShadowWidth > 0) {
		tempCtx.beginPath();
		tempCtx.rect(0, 0, tempCanvas.width, tempCanvas.height);
		if (selectRectRadius > 0) {
			tempCtx.roundRect(
				offsetX,
				offsetY,
				contentWidth,
				contentHeight,
				selectRectRadius,
			);
		} else {
			tempCtx.rect(offsetX, offsetY, contentWidth, contentHeight);
		}
		tempCtx.clip("evenodd");

		tempCtx.shadowBlur = selectRectShadowWidth;
		tempCtx.shadowColor = selectRectShadowColor;
		tempCtx.fillStyle = selectRectShadowColor;

		tempCtx.beginPath();

		if (selectRectRadius > 0) {
			tempCtx.roundRect(
				offsetX,
				offsetY,
				contentWidth,
				contentHeight,
				selectRectRadius,
			);
		} else {
			tempCtx.rect(offsetX, offsetY, contentWidth, contentHeight);
		}

		tempCtx.fill();
	}

	return tempCanvas;
};

/**
 * 保存截图到指定文件
 */
export const saveToFile = async (
	appSettings: AppSettingsData,
	imageCanvas: HTMLCanvasElement | undefined,
	beforeSaveFile?: (filePath: string) => Promise<void>,
	prevImageFormat?: ImageFormat,
	fastSavePath?: ImagePath,
) => {
	const imagePath =
		fastSavePath ?? (await showImageDialog(appSettings, prevImageFormat));

	if (!imagePath) {
		return;
	}

	const imageDataPromise = new Promise<HTMLCanvasElement | undefined>(
		(resolve) => {
			resolve(imageCanvas);
		},
	)
		.then(async (canvas) => {
			if (!canvas) {
				return;
			}

			let blobType: string = imagePath.imageFormat;
			if (
				imagePath.imageFormat === ImageFormat.AVIF ||
				imagePath.imageFormat === ImageFormat.JPEG_XL
			) {
				blobType = "image/webp";
			}

			return new Promise<Blob | null>((resolve) => {
				canvas.toBlob(resolve, blobType, 1);
			});
		})
		.then((blob) => {
			if (!blob) {
				return;
			}

			if (blob instanceof Blob) {
				return blob.arrayBuffer();
			}

			return blob;
		});

	if (beforeSaveFile) {
		await beforeSaveFile(imagePath.filePath);
	}

	const imageData = await imageDataPromise;

	if (!imageData) {
		appError("[saveToFile] imageData is undefined");
		return;
	}

	await saveFile(imagePath.filePath, imageData, imagePath.imageFormat);
};

export const fixedToScreen = async (
	captureBoundingBoxInfo: CaptureBoundingBoxInfo,
	appWindow: AppWindow,
	layerContainerElement: HTMLDivElement,
	selectLayerAction: SelectLayerActionType,
	fixedContentAction: FixedContentActionType,
	drawLayerAction: DrawLayerActionType,
	drawCacheLayerAction: DrawCacheLayerActionType,
	setCaptureStep: (step: CaptureStep) => void,
	/** 已有的 OCR 结果 */
	ocrResult: AppOcrResult | undefined,
	saveCaptureHistory: (canvas: HTMLCanvasElement) => void,
) => {
	// 创建一个固定的图片
	const selectRectParams = selectLayerAction.getSelectRectParams();
	if (!selectRectParams) {
		return;
	}

	layerContainerElement.style.opacity = "0";
	layerContainerElement.style.width = "100%";
	layerContainerElement.style.height = "100%";

	const canvas = await getCanvas(
		selectRectParams,
		drawLayerAction,
		drawCacheLayerAction,
	);
	if (!canvas) {
		return;
	}

	saveCaptureHistory(canvas);

	setCaptureStep(CaptureStep.Fixed);
	createDrawWindow();

	await Promise.all([
		appWindow.hide(),
		appWindow.setTitle("Snow Shot - Fixed Content"),
	]);

	await setWindowRect(
		appWindow,
		captureBoundingBoxInfo.transformWindowRect({
			min_x: selectRectParams.rect.min_x - selectRectParams.shadowWidth,
			min_y: selectRectParams.rect.min_y - selectRectParams.shadowWidth,
			max_x: selectRectParams.rect.max_x + selectRectParams.shadowWidth,
			max_y: selectRectParams.rect.max_y + selectRectParams.shadowWidth,
		}),
	);
	setExcludeFromCapture(false);
	await Promise.all([
		appWindow.show(),
		appWindow.setAlwaysOnTop(true),
		fixedContentAction.init({
			canvas,
			captureBoundingBoxInfo,
			ocrResult,
			selectRectParams,
		}),
	]);

	// 简单加个过渡效果
	layerContainerElement.style.transition = "opacity 0.3s ease-in-out";

	// 等待两帧，让窗口内容显示出来
	await new Promise((resolve) => {
		setTimeout(resolve, 17);
	});

	layerContainerElement.style.opacity = "1";
};

export const copyToClipboard = async (
	imageData: Blob | ArrayBuffer,
	appSettings: AppSettingsData,
	selectRectParams: SelectRectParams | undefined,
) => {
	let imageDataArrayBuffer: ArrayBuffer | undefined;
	if (
		getPlatform() === "windows" &&
		appSettings[AppSettingsGroup.SystemScreenshot]
			.tryWriteBitmapImageToClipboard &&
		selectRectParams &&
		selectRectParams.shadowWidth === 0 &&
		selectRectParams.radius === 0
	) {
		try {
			imageDataArrayBuffer =
				imageData instanceof Blob ? await imageData.arrayBuffer() : imageData;
			await writeBitmapImageToClipboard(imageDataArrayBuffer);

			// 写入成功后直接返回
			return;
		} catch {
			appWarn("[copyToClipboard] writeBitmapImageToClipboard error");
		}
	}

	// 尝试使用浏览器剪贴板写入，浏览器写入更快
	let browserClipboardWriteSuccess = false;
	try {
		if (appSettings[AppSettingsGroup.SystemScreenshot].enableBrowserClipboard) {
			if (
				"clipboard" in navigator &&
				"write" in navigator.clipboard &&
				getPlatform() !== "macos"
			) {
				if (window.isSecureContext && window.document.hasFocus()) {
					await navigator.clipboard.write([
						new ClipboardItem({
							"image/png":
								imageData instanceof Blob ? imageData : new Blob([imageData]),
						}),
					]);
					browserClipboardWriteSuccess = true;
				}
			}
		}
	} catch {
		browserClipboardWriteSuccess = false;
	}

	if (!browserClipboardWriteSuccess) {
		await writeImageToClipboard(imageDataArrayBuffer ?? imageData);
	}
};

export const handleOcrDetect = async (
	captureBoundingBoxInfo: CaptureBoundingBoxInfo,
	selectLayerAction: SelectLayerActionType,
	drawLayerAction: DrawLayerActionType,
	drawCacheLayerAction: DrawCacheLayerActionType,
	ocrBlocksAction: OcrBlocksActionType,
	/** 忽略如圆角、阴影等样式 */
	ignoreStyle?: boolean,
) => {
	const selectRectParams = selectLayerAction.getSelectRectParams();
	if (!selectRectParams) {
		return;
	}

	const { rect: selectRect } = selectRectParams;

	if (!selectRect) {
		return;
	}

	const imageCanvas = await getCanvas(
		selectRectParams,
		drawLayerAction,
		drawCacheLayerAction,
		ignoreStyle,
	);
	if (!imageCanvas) {
		return;
	}

	await ocrBlocksAction.init(
		selectRect,
		captureBoundingBoxInfo,
		imageCanvas,
		undefined,
	);
};
