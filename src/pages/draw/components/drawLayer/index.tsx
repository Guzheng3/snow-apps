"use client";

import { convertFileSrc } from "@tauri-apps/api/core";
import type * as PIXI from "pixi.js";
import type React from "react";
import {
	useCallback,
	useEffect,
	useImperativeHandle,
	useRef,
	useState,
} from "react";
import type { ElementRect, ImageBuffer } from "@/types/commands/screenshot";
import type { CaptureHistoryItem } from "@/utils/appStore";
import { getCaptureHistoryImageAbsPath } from "@/utils/captureHistory";
import { supportOffscreenCanvas } from "@/utils/environment";
import type { CaptureBoundingBoxInfo } from "../../extra";
import type { ImageSharedBufferData } from "../../tools";
import { defaultWatermarkProps } from "../drawToolbar/components/tools/drawExtraTool/components/watermarkTool";
import {
	addImageToContainerAction,
	canvasRenderAction,
	clearCanvasAction,
	clearContainerAction,
	clearContextAction,
	createBlurSpriteAction,
	createNewCanvasContainerAction,
	deleteBlurSpriteAction,
	disposeCanvasAction,
	getImageDataAction,
	INIT_CONTAINER_KEY,
	initCanvasAction,
	renderToCanvasAction,
	resizeCanvasAction,
	updateBlurSpriteAction,
	updateHighlightAction,
	updateHighlightElementAction,
	updateWatermarkSpriteAction,
} from "./actions";
import type {
	BlurSprite,
	BlurSpriteProps,
	HighlightElement,
	HighlightElementProps,
	HighlightProps,
	WatermarkProps,
} from "./baseLayerRenderActions";

export type DrawLayerActionType = {
	/**
	 * 初始化画布
	 */
	initCanvas: (antialias: boolean) => Promise<void>;
	resizeCanvas: (width: number, height: number) => void;
	clearCanvas: () => Promise<void>;
	getLayerContainerElement: () => HTMLDivElement | null;
	changeCursor: (cursor: Required<React.CSSProperties>["cursor"]) => string;
	getImageData: (selectRect: ElementRect) => Promise<ImageData | undefined>;
	/**
	 * 渲染画布
	 */
	renderToCanvas: (
		selectRect: ElementRect,
	) => Promise<PIXI.ICanvas | undefined>;
	/**
	 * 渲染画布
	 */
	canvasRender: () => Promise<void>;
	/**
	 * 添加图片到画布容器
	 */
	addImageToContainer: (
		containerKey: string,
		imageSrc: string | ImageSharedBufferData,
	) => Promise<void>;
	/**
	 * 清空画布容器
	 */
	clearContainer: (containerKey: string) => Promise<void>;
	/**
	 * 创建模糊效果
	 */
	createBlurSprite: (
		blurContainerKey: string,
		blurElementId: string,
	) => Promise<void>;
	/**
	 * 更新模糊效果
	 */
	updateBlurSprite: (
		blurElementId: string,
		blurProps: BlurSpriteProps,
		updateFilter: boolean,
	) => Promise<void>;
	/**
	 * 更新水印效果
	 */
	updateWatermarkSprite: (watermarkProps: WatermarkProps) => Promise<void>;
	/**
	 * 删除模糊效果
	 */
	deleteBlurSprite: (blurElementId: string) => Promise<void>;
	/**
	 * 更新 highlight 元素
	 */
	updateHighlightElement: (
		highlightContainerKey: string,
		highlightElementId: string,
		highlightElementProps: HighlightElementProps | undefined,
	) => Promise<void>;
	/**
	 * 重新渲染 highlight
	 */
	updateHighlight: (
		highlightContainerKey: string,
		highlightProps: HighlightProps,
	) => Promise<void>;
	/**
	 * 清除现有的状态
	 */
	clearContext: () => Promise<void>;
	switchCaptureHistory: (item: CaptureHistoryItem | undefined) => Promise<void>;
	setEnable: (enable: boolean) => void;
	/**
	 * 执行截图
	 */
	onExecuteScreenshot: () => Promise<void>;
	/**
	 * 截图准备
	 */
	onCaptureReady: (
		imageSrc: string | undefined,
		imageBuffer: ImageBuffer | ImageSharedBufferData | undefined,
		captureBoundingBoxInfo: CaptureBoundingBoxInfo,
	) => Promise<void>;
	/**
	 * 显示器信息准备
	 */
	onCaptureBoundingBoxInfoReady: (
		captureBoundingBoxInfo: CaptureBoundingBoxInfo,
	) => Promise<void>;
	/**
	 * 截图加载完成
	 */
	onCaptureLoad: (
		imageSrc: string | undefined,
		imageBuffer: ImageBuffer | ImageSharedBufferData | undefined,
		captureBoundingBoxInfo: CaptureBoundingBoxInfo,
	) => Promise<void>;
	/**
	 * 截图完成
	 */
	onCaptureFinish: () => Promise<void>;
};

export type DrawLayerProps = {
	zIndex: number;
	onInitCanvasReady: () => Promise<void>;
	actionRef: React.RefObject<DrawLayerActionType | undefined>;
};

export const DRAW_LAYER_BLUR_CONTAINER_KEY = "draw_layer_blur_container";
export const DRAW_LAYER_HIGHLIGHT_CONTAINER_KEY =
	"draw_layer_highlight_container";
export const DRAW_LAYER_WATERMARK_CONTAINER_KEY =
	"draw_layer_watermark_container";

export const DrawLayer: React.FC<DrawLayerProps> = ({
	zIndex,
	onInitCanvasReady,
	actionRef,
}) => {
	const layerContainerElementRef = useRef<HTMLDivElement>(null);
	/** 可能的 OffscreenCanvas，用于在 Web Worker 中渲染 */
	const offscreenCanvasRef = useRef<OffscreenCanvas | undefined>(undefined);
	const canvasAppRef = useRef<PIXI.Application | undefined>(undefined);
	const canvasContainerMapRef = useRef<Map<string, PIXI.Container>>(new Map());
	const canvasContainerChildCountRef = useRef<number>(0);
	const currentImageTextureRef = useRef<PIXI.Texture | undefined>(undefined);
	const blurSpriteMapRef = useRef<Map<string, BlurSprite>>(new Map());
	const highlightElementMapRef = useRef<Map<string, HighlightElement>>(
		new Map(),
	);
	const lastWatermarkPropsRef = useRef<WatermarkProps>(defaultWatermarkProps);
	const [rendererWorker, setRendererWorker] = useState<Worker | undefined>(
		undefined,
	);

	useEffect(() => {
		const worker = supportOffscreenCanvas()
			? new Worker(new URL("./workers/renderWorker.ts", import.meta.url))
			: undefined;
		setRendererWorker(worker);
		return () => {
			worker?.terminate();
		};
	}, []);

	/** 创建一个新的画布容器 */
	const createNewCanvasContainer = useCallback(
		async (containerKey: string): Promise<string | undefined> => {
			return await createNewCanvasContainerAction(
				rendererWorker,
				canvasAppRef,
				canvasContainerMapRef,
				containerKey,
			);
		},
		[rendererWorker],
	);

	const disposeCanvas = useCallback(async () => {
		await disposeCanvasAction(rendererWorker, canvasAppRef);
	}, [rendererWorker]);

	/** 初始化画布 */
	const initCanvas = useCallback<DrawLayerActionType["initCanvas"]>(
		async (antialias: boolean) => {
			const canvas = document.createElement("canvas");
			if (layerContainerElementRef.current) {
				if (layerContainerElementRef.current.firstChild) {
					layerContainerElementRef.current.removeChild(
						layerContainerElementRef.current.firstChild,
					);
				}
				layerContainerElementRef.current.appendChild(canvas);
			}

			offscreenCanvasRef.current = supportOffscreenCanvas()
				? canvas.transferControlToOffscreen()
				: undefined;

			const initOptions: Partial<PIXI.ApplicationOptions> = {
				backgroundAlpha: 0,
				eventFeatures: {
					move: false,
					globalMove: false,
					click: false,
					wheel: false,
				},
				autoStart: false,
				antialias,
				canvas: offscreenCanvasRef.current ?? canvas,
				preference: "webgl",
			};

			await initCanvasAction(
				rendererWorker,
				canvasAppRef,
				initOptions,
				offscreenCanvasRef.current ? [offscreenCanvasRef.current] : undefined,
			);

			await onInitCanvasReady();
		},
		[rendererWorker, onInitCanvasReady],
	);

	useEffect(() => {
		initCanvas(true);
	}, [initCanvas]);

	/** 调整画布大小 */
	const resizeCanvas = useCallback(
		async (width: number, height: number) => {
			await createNewCanvasContainer(INIT_CONTAINER_KEY);
			await resizeCanvasAction(rendererWorker, canvasAppRef, width, height);
		},
		[createNewCanvasContainer, rendererWorker],
	);

	const clearCanvas = useCallback<
		DrawLayerActionType["clearCanvas"]
	>(async () => {
		await clearCanvasAction(
			rendererWorker,
			canvasAppRef,
			canvasContainerMapRef,
			canvasContainerChildCountRef,
		);
	}, [rendererWorker]);

	const changeCursor = useCallback<DrawLayerActionType["changeCursor"]>(
		(cursor) => {
			if (!layerContainerElementRef.current) {
				return "auto";
			}

			const previousCursor = layerContainerElementRef.current.style.cursor;
			layerContainerElementRef.current.style.cursor = cursor;
			return previousCursor;
		},
		[],
	);

	const getLayerContainerElement = useCallback<
		DrawLayerActionType["getLayerContainerElement"]
	>(() => layerContainerElementRef.current, []);

	const getImageData = useCallback<DrawLayerActionType["getImageData"]>(
		async (selectRect: ElementRect) => {
			return getImageDataAction(rendererWorker, canvasAppRef, selectRect);
		},
		[rendererWorker],
	);

	const renderToCanvas = useCallback<DrawLayerActionType["renderToCanvas"]>(
		async (selectRect: ElementRect) => {
			return renderToCanvasAction(rendererWorker, canvasAppRef, selectRect);
		},
		[rendererWorker],
	);

	const canvasRender = useCallback<
		DrawLayerActionType["canvasRender"]
	>(async () => {
		await canvasRenderAction(rendererWorker, canvasAppRef);
	}, [rendererWorker]);

	const addImageToContainer = useCallback<
		DrawLayerActionType["addImageToContainer"]
	>(
		async (containerKey: string, imageSrc: string | ImageSharedBufferData) => {
			await addImageToContainerAction(
				rendererWorker,
				canvasContainerMapRef,
				currentImageTextureRef,
				containerKey,
				imageSrc,
			);
		},
		[rendererWorker],
	);

	const clearContainer = useCallback<DrawLayerActionType["clearContainer"]>(
		async (containerKey: string) => {
			await clearContainerAction(
				rendererWorker,
				canvasContainerMapRef,
				containerKey,
			);
		},
		[rendererWorker],
	);

	const createBlurSprite = useCallback<DrawLayerActionType["createBlurSprite"]>(
		async (blurContainerKey: string, blurElementId: string) => {
			await createBlurSpriteAction(
				rendererWorker,
				canvasContainerMapRef,
				currentImageTextureRef,
				blurSpriteMapRef,
				blurContainerKey,
				blurElementId,
			);
		},
		[rendererWorker],
	);

	const updateBlurSprite = useCallback<DrawLayerActionType["updateBlurSprite"]>(
		async (
			blurElementId: string,
			blurProps: BlurSpriteProps,
			updateFilter: boolean,
		) => {
			await updateBlurSpriteAction(
				rendererWorker,
				blurSpriteMapRef,
				blurElementId,
				blurProps,
				updateFilter,
				window.devicePixelRatio,
			);
		},
		[rendererWorker],
	);

	const deleteBlurSprite = useCallback<DrawLayerActionType["deleteBlurSprite"]>(
		async (blurElementId: string) => {
			await deleteBlurSpriteAction(
				rendererWorker,
				blurSpriteMapRef,
				blurElementId,
			);
		},
		[rendererWorker],
	);

	const updateWatermarkSprite = useCallback<
		DrawLayerActionType["updateWatermarkSprite"]
	>(
		async (watermarkProps: WatermarkProps) => {
			await updateWatermarkSpriteAction(
				rendererWorker,
				canvasAppRef,
				canvasContainerMapRef,
				lastWatermarkPropsRef,
				DRAW_LAYER_WATERMARK_CONTAINER_KEY,
				watermarkProps,
				window.devicePixelRatio,
			);
		},
		[rendererWorker],
	);

	const updateHighlightElement = useCallback<
		DrawLayerActionType["updateHighlightElement"]
	>(
		async (
			highlightContainerKey: string,
			highlightElementId: string,
			highlightElementProps: HighlightElementProps | undefined,
		) => {
			await updateHighlightElementAction(
				rendererWorker,
				canvasContainerMapRef,
				highlightElementMapRef,
				highlightContainerKey,
				highlightElementId,
				highlightElementProps,
				window.devicePixelRatio,
			);
		},
		[rendererWorker],
	);

	const updateHighlight = useCallback<DrawLayerActionType["updateHighlight"]>(
		async (highlightContainerKey: string, highlightProps: HighlightProps) => {
			await updateHighlightAction(
				rendererWorker,
				canvasContainerMapRef,
				highlightElementMapRef,
				highlightContainerKey,
				highlightProps,
			);
		},
		[rendererWorker],
	);

	const clearContext = useCallback<
		DrawLayerActionType["clearContext"]
	>(async () => {
		await clearContextAction(
			rendererWorker,
			blurSpriteMapRef,
			highlightElementMapRef,
			lastWatermarkPropsRef,
		);
	}, [rendererWorker]);

	useEffect(() => {
		return () => {
			disposeCanvas();
		};
	}, [disposeCanvas]);

	const currentCaptureImageSrcRef = useRef<
		string | ImageSharedBufferData | undefined
	>(undefined);
	const blurContainerKeyRef = useRef<string | undefined>(undefined);
	const highlightContainerKeyRef = useRef<string | undefined>(undefined);
	const watermarkContainerKeyRef = useRef<string | undefined>(undefined);
	/*
	 * 初始化截图
	 */
	const onCaptureReady = useCallback<DrawLayerActionType["onCaptureReady"]>(
		async (
			imageSrc: string | undefined,
			imageBuffer: ImageBuffer | ImageSharedBufferData | undefined,
		): Promise<void> => {
			// 底图作为单独的层级显示
			const isSharedBuffer = imageBuffer && "sharedBuffer" in imageBuffer;
			currentCaptureImageSrcRef.current = isSharedBuffer
				? imageBuffer
				: imageSrc;
			// 可能是切换截图历史，这种情况下不存在截图数据
			if (imageSrc) {
				await addImageToContainer(INIT_CONTAINER_KEY, imageSrc);
			} else if (isSharedBuffer) {
				await addImageToContainer(INIT_CONTAINER_KEY, imageBuffer);
			}
			// 水印层
			watermarkContainerKeyRef.current = await createNewCanvasContainer(
				DRAW_LAYER_WATERMARK_CONTAINER_KEY,
			);
			// 模糊层
			blurContainerKeyRef.current = await createNewCanvasContainer(
				DRAW_LAYER_BLUR_CONTAINER_KEY,
			);
			// 高亮层
			highlightContainerKeyRef.current = await createNewCanvasContainer(
				DRAW_LAYER_HIGHLIGHT_CONTAINER_KEY,
			);

			await canvasRender();
		},
		[createNewCanvasContainer, canvasRender, addImageToContainer],
	);

	const onCaptureFinish = useCallback<
		DrawLayerActionType["onCaptureFinish"]
	>(async () => {
		await clearCanvas();
	}, [clearCanvas]);

	const onExecuteScreenshot = useCallback<
		DrawLayerActionType["onExecuteScreenshot"]
	>(async () => {}, []);

	const onCaptureBoundingBoxInfoReady = useCallback(
		async (
			...args: Parameters<DrawLayerActionType["onCaptureBoundingBoxInfoReady"]>
		) => {
			const [captureBoundingBoxInfo] = args;

			// 将画布调整为截图大小
			const { width, height } = captureBoundingBoxInfo;

			resizeCanvas(width, height);
		},
		[resizeCanvas],
	);

	const onCaptureLoad = useCallback<
		DrawLayerActionType["onCaptureLoad"]
	>(async () => {}, []);

	const switchCaptureHistory = useCallback(
		async (item: CaptureHistoryItem | undefined) => {
			if (!item) {
				if (currentCaptureImageSrcRef.current) {
					await addImageToContainer(
						INIT_CONTAINER_KEY,
						currentCaptureImageSrcRef.current,
					);
				}
			} else {
				const fileUri = convertFileSrc(
					await getCaptureHistoryImageAbsPath(item.file_name),
				);
				await addImageToContainer(INIT_CONTAINER_KEY, fileUri);
			}

			// 移除之前显示的已有内容
			await Promise.all([
				clearContext(),
				clearContainer(DRAW_LAYER_HIGHLIGHT_CONTAINER_KEY),
				clearContainer(DRAW_LAYER_BLUR_CONTAINER_KEY),
				clearContainer(DRAW_LAYER_WATERMARK_CONTAINER_KEY),
			]);

			await canvasRender();
		},
		[addImageToContainer, canvasRender, clearContainer, clearContext],
	);

	const [enable, setEnable] = useState(false);

	useEffect(() => {
		if (!layerContainerElementRef.current) {
			return;
		}

		if (enable) {
			layerContainerElementRef.current.style.pointerEvents = "auto";
		} else {
			layerContainerElementRef.current.style.pointerEvents = "none";
		}
	}, [enable]);

	useImperativeHandle(
		actionRef,
		() => ({
			resizeCanvas,
			clearCanvas,
			getLayerContainerElement,
			createNewCanvasContainer,
			getImageData,
			renderToCanvas,
			initCanvas,
			changeCursor,
			canvasRender,
			addImageToContainer,
			clearContainer,
			createBlurSprite,
			updateBlurSprite,
			deleteBlurSprite,
			updateWatermarkSprite,
			updateHighlightElement,
			updateHighlight,
			clearContext,
			switchCaptureHistory,
			setEnable,
			onExecuteScreenshot,
			onCaptureReady,
			onCaptureFinish,
			onCaptureBoundingBoxInfoReady,
			onCaptureLoad,
		}),
		[
			resizeCanvas,
			clearCanvas,
			getLayerContainerElement,
			createNewCanvasContainer,
			getImageData,
			renderToCanvas,
			initCanvas,
			changeCursor,
			canvasRender,
			addImageToContainer,
			clearContainer,
			createBlurSprite,
			updateBlurSprite,
			deleteBlurSprite,
			updateWatermarkSprite,
			updateHighlightElement,
			updateHighlight,
			clearContext,
			switchCaptureHistory,
			onCaptureFinish,
			onCaptureBoundingBoxInfoReady,
			onExecuteScreenshot,
			onCaptureReady,
			onCaptureLoad,
		],
	);

	return (
		<>
			<div
				className="base-layer"
				ref={layerContainerElementRef}
				style={{ zIndex }}
			></div>

			<style jsx>
				{`
                .base-layer {
                    position: absolute;
                    top: 0;
                    left: 0;
                    width: 100%;
                    height: 100%;
                }

                .base-layer :global(canvas) {
                    width: 100%;
                    height: 100%;
                }`}
			</style>
		</>
	);
};
