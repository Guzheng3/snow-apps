import * as htmlToImage from "html-to-image";
import type { RefObject } from "react";
import { appError } from "@/utils/log";
import { type FixedContentProcessImageConfig, FixedContentType } from ".";
import type { FixedContentCoreDrawActionType } from "./components/drawLayer";
import { needSwapWidthAndHeight } from "./extra";

/**
 * 应用旋转和翻转变换到 Canvas 2D 上下文
 * @param context Canvas 2D 上下文
 * @param processImageConfig 图像处理配置（包含旋转角度和翻转信息）
 * @param canvasWidth canvas 宽度
 * @param canvasHeight canvas 高度
 */
const applyRotationTransform = (
	context: CanvasRenderingContext2D,
	processImageConfig: FixedContentProcessImageConfig,
	canvasWidth: number,
	canvasHeight: number,
) => {
	const angle = processImageConfig.angle;

	switch (angle) {
		case 1: // 90度旋转
			context.translate(canvasWidth, 0);
			context.rotate((90 * Math.PI) / 180);
			break;
		case 2: // 180度旋转
			context.translate(canvasWidth, canvasHeight);
			context.rotate((180 * Math.PI) / 180);
			break;
		case 3: // 270度旋转
			context.translate(0, canvasHeight);
			context.rotate((270 * Math.PI) / 180);
			break;
		// case 0 或其他情况：不需要变换
	}

	// 处理翻转
	if (processImageConfig.horizontalFlip || processImageConfig.verticalFlip) {
		// 获取当前坐标系的尺寸（旋转后可能宽高已交换）
		const currentWidth = needSwapWidthAndHeight(angle)
			? canvasHeight
			: canvasWidth;
		const currentHeight = needSwapWidthAndHeight(angle)
			? canvasWidth
			: canvasHeight;

		// 应用翻转
		const scaleX = processImageConfig.horizontalFlip ? -1 : 1;
		const scaleY = processImageConfig.verticalFlip ? -1 : 1;

		// 如果翻转，需要先平移，再缩放
		if (processImageConfig.horizontalFlip) {
			context.translate(currentWidth, 0);
		}
		if (processImageConfig.verticalFlip) {
			context.translate(0, currentHeight);
		}

		context.scale(scaleX, scaleY);
	}
};

export const renderToCanvasAction = async (
	fixedContentTypeRef: RefObject<FixedContentType | undefined>,
	canvasElementRef: RefObject<HTMLCanvasElement | undefined>,
	imageRef: RefObject<HTMLImageElement | null>,
	htmlContentContainerRef: RefObject<HTMLIFrameElement | null>,
	textContentContainerRef: RefObject<HTMLDivElement | null>,
	drawActionRef: RefObject<FixedContentCoreDrawActionType | undefined>,
	processImageConfigRef: RefObject<FixedContentProcessImageConfig>,
	ignoreDrawCanvas: boolean = false,
) => {
	let canvas: HTMLCanvasElement | undefined;

	if (fixedContentTypeRef.current === FixedContentType.DrawCanvas) {
		if (!canvasElementRef.current) {
			appError("[renderToCanvas] canvasElementRef is undefined");
			return;
		}

		const sourceCanvas = canvasElementRef.current;
		canvas = document.createElement("canvas");

		// 根据旋转角度设置 canvas 尺寸
		if (needSwapWidthAndHeight(processImageConfigRef.current.angle)) {
			canvas.width = sourceCanvas.height;
			canvas.height = sourceCanvas.width;
		} else {
			canvas.width = sourceCanvas.width;
			canvas.height = sourceCanvas.height;
		}

		const context = canvas.getContext("2d");
		if (!context) {
			return;
		}

		// 保存当前上下文状态
		context.save();

		// 应用旋转和翻转变换
		applyRotationTransform(
			context,
			processImageConfigRef.current,
			canvas.width,
			canvas.height,
		);

		// 绘制源 canvas（旋转后的坐标系中绘制）
		context.drawImage(
			sourceCanvas,
			0,
			0,
			sourceCanvas.width,
			sourceCanvas.height,
		);

		// 恢复上下文状态
		context.restore();
	} else if (fixedContentTypeRef.current === FixedContentType.Image) {
		if (!imageRef.current) {
			appError("[renderToCanvas] imageRef is undefined");
			return;
		}

		canvas = document.createElement("canvas");
		if (needSwapWidthAndHeight(processImageConfigRef.current.angle)) {
			canvas.width = imageRef.current.naturalHeight;
			canvas.height = imageRef.current.naturalWidth;
		} else {
			canvas.width = imageRef.current.naturalWidth;
			canvas.height = imageRef.current.naturalHeight;
		}

		const context = canvas.getContext("2d");
		if (!context) {
			return;
		}

		// 保存当前上下文状态
		context.save();

		// 应用旋转和翻转变换
		applyRotationTransform(
			context,
			processImageConfigRef.current,
			canvas.width,
			canvas.height,
		);

		// 绘制图片（旋转后的坐标系中绘制）
		context.drawImage(
			imageRef.current,
			0,
			0,
			imageRef.current.naturalWidth,
			imageRef.current.naturalHeight,
		);

		// 恢复上下文状态
		context.restore();
	} else {
		let htmlElement: HTMLElement | undefined | null;
		if (fixedContentTypeRef.current === FixedContentType.Html) {
			htmlElement =
				htmlContentContainerRef.current?.contentWindow?.document.body;
		} else if (fixedContentTypeRef.current === FixedContentType.Text) {
			htmlElement = textContentContainerRef.current;
		}

		if (!htmlElement) {
			appError("[renderToCanvas] htmlElement is undefined");
			return;
		}

		const sourceCanvas = await htmlToImage.toCanvas(htmlElement);
		const { angle, horizontalFlip, verticalFlip } =
			processImageConfigRef.current;

		// 若存在旋转或翻转，需要重新绘制到新的 canvas 上（仅无旋转且无翻转时可直接复用）
		if (angle !== 0 || horizontalFlip || verticalFlip) {
			canvas = document.createElement("canvas");

			// 根据旋转角度设置 canvas 尺寸
			if (needSwapWidthAndHeight(angle)) {
				canvas.width = sourceCanvas.height;
				canvas.height = sourceCanvas.width;
			} else {
				canvas.width = sourceCanvas.width;
				canvas.height = sourceCanvas.height;
			}

			const context = canvas.getContext("2d");
			if (!context) {
				return;
			}

			// 保存上下文状态并应用旋转和翻转变换
			context.save();
			applyRotationTransform(
				context,
				processImageConfigRef.current,
				canvas.width,
				canvas.height,
			);

			// 绘制源 canvas
			context.drawImage(sourceCanvas, 0, 0);
			context.restore();
		} else {
			canvas = sourceCanvas;
		}
	}

	const context = canvas.getContext("2d");
	if (!context) {
		appError("[renderToCanvas] context is undefined");
		return;
	}

	const drawCanvas = drawActionRef.current?.getCanvas();
	if (drawCanvas && !ignoreDrawCanvas) {
		context.drawImage(drawCanvas, 0, 0, canvas.width, canvas.height);
	}

	return canvas;
};
