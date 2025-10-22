import type { RefObject } from "react";
import type { ElementRect } from "@/types/commands/screenshot";
import { appError } from "@/utils/log";
import type { FixedContentProcessImageConfig } from ".";
import type { FixedContentCoreDrawActionType } from "./components/drawLayer";
import type { FixedContentImageLayerActionType } from "./components/imageLayer";
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
	imageLayerActionRef: RefObject<FixedContentImageLayerActionType | undefined>,
	drawActionRef: RefObject<FixedContentCoreDrawActionType | undefined>,
	processImageConfigRef: RefObject<FixedContentProcessImageConfig>,
	ignoreDrawCanvas: boolean = false,
	renderRect: ElementRect,
) => {
	const imageLayerAction = imageLayerActionRef.current?.getImageLayerAction();
	if (!imageLayerAction) {
		appError("[renderToCanvas] imageLayerAction is undefined");
		return;
	}

	const sourceBitmap = await imageLayerAction.getImageBitmap(renderRect);
	if (!sourceBitmap) {
		appError("[renderToCanvas] sourceBitmap is undefined");
		return;
	}

	const canvas = document.createElement("canvas");

	// 根据旋转角度设置 canvas 尺寸
	if (needSwapWidthAndHeight(processImageConfigRef.current.angle)) {
		canvas.width = sourceBitmap.height;
		canvas.height = sourceBitmap.width;
	} else {
		canvas.width = sourceBitmap.width;
		canvas.height = sourceBitmap.height;
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
		sourceBitmap,
		0,
		0,
		sourceBitmap.width,
		sourceBitmap.height,
	);

	// 恢复上下文状态
	context.restore();

	const drawCanvas = drawActionRef.current?.getCanvas();
	if (drawCanvas && !ignoreDrawCanvas) {
		context.drawImage(drawCanvas, 0, 0, canvas.width, canvas.height);
	}

	return canvas;
};
