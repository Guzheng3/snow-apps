import type { Application, ApplicationOptions } from "pixi.js";
import * as PIXI from "pixi.js";
import * as PIXIFilters from "pixi-filters";
import type { RefObject } from "react";
import type { SelectRectParams } from "@/pages/draw/components/selectLayer";
import type { ImageSharedBufferData } from "@/pages/draw/tools";
import type { ElementRect } from "@/types/commands/screenshot";
import type { RefWrap } from "./workers/renderWorkerTypes";

export type RefType<T> = RefWrap<T> | RefObject<T>;

export const renderInitBaseImageTextureAction = async (
	baseImageTextureRef: RefType<PIXI.Texture | undefined>,
	imageUrl: string,
): Promise<{ width: number; height: number }> => {
	const texture = await PIXI.Assets.load<PIXI.Texture>({
		src: imageUrl,
		parser: "texture",
	});
	baseImageTextureRef.current = texture;
	return { width: texture.width, height: texture.height };
};

export const renderDisposeCanvasAction = (
	canvasAppRef: RefType<Application | undefined>,
) => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}
	canvasApp.destroy(true, true);
	canvasAppRef.current = undefined;
};

export const renderInitCanvasAction = async (
	canvasAppRef: RefType<Application | undefined>,
	appOptions: Partial<ApplicationOptions>,
): Promise<OffscreenCanvas | HTMLCanvasElement | undefined> => {
	renderDisposeCanvasAction(canvasAppRef);

	const canvasApp = new PIXI.Application();
	await canvasApp.init({
		...appOptions,
	});
	canvasAppRef.current = canvasApp;
	return canvasApp.canvas;
};

export const renderCreateNewCanvasContainerAction = (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerListRef: RefType<Map<string, PIXI.Container>>,
	containerKey: string,
) => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	const container = new PIXI.Container();
	container.zIndex = canvasContainerListRef.current.size + 1;
	container.sortableChildren = true;
	container.x = 0;
	container.y = 0;
	canvasApp.stage.addChild(container);
	canvasContainerListRef.current.set(containerKey, container);

	return containerKey;
};

export const renderResizeCanvasAction = (
	canvasAppRef: RefType<Application | undefined>,
	width: number,
	height: number,
) => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	canvasApp.renderer.resize(width, height);
};

export const renderClearCanvasAction = (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	canvasContainerChildCountRef: RefType<number>,
	currentImageTextureRef: RefType<PIXI.Texture | undefined>,
	baseImageTextureRef: RefType<PIXI.Texture | undefined>,
) => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}
	while (canvasApp.stage.children[0]) {
		canvasApp.stage.removeChild(canvasApp.stage.children[0]);
	}
	canvasContainerMapRef.current.clear();
	canvasContainerChildCountRef.current = 0;
	currentImageTextureRef.current = undefined;
	baseImageTextureRef.current = undefined;

	canvasApp.render();
};

export const renderGetImageBitmapAction = async (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	imageContainerKey: string,
	selectRect: ElementRect | undefined,
	renderContainerKey: string | undefined,
): Promise<ImageBitmap | undefined> => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	let renderContainer = canvasApp.stage;
	if (renderContainerKey) {
		const container = canvasContainerMapRef.current.get(renderContainerKey);
		if (container) {
			renderContainer = container;
		}
	}

	const imageContainer = canvasContainerMapRef.current.get(imageContainerKey);
	let hasChangeAlpha = false;
	if (imageContainer?.children[0] && imageContainer.children[0].alpha === 0) {
		imageContainer.children[0].alpha = 1;
		hasChangeAlpha = true;
	}

	const canvas = canvasApp.renderer.extract.canvas({
		target: renderContainer,
		frame: selectRect
			? new PIXI.Rectangle(
					selectRect.min_x,
					selectRect.min_y,
					selectRect.max_x - selectRect.min_x,
					selectRect.max_y - selectRect.min_y,
				)
			: undefined,
	});

	if (imageContainer?.children[0] && hasChangeAlpha) {
		imageContainer.children[0].alpha = 0;
	}

	const result = await self.createImageBitmap(canvas as OffscreenCanvas);
	return result;
};

export const renderRenderToCanvasAction = (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	imageContainerKey: string,
	selectRect: ElementRect,
	containerId: string | undefined,
): PIXI.ICanvas | undefined => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	const imageContainer = canvasContainerMapRef.current.get(imageContainerKey);
	let hasChangeAlpha = false;
	if (imageContainer?.children[0] && imageContainer.children[0].alpha === 0) {
		imageContainer.children[0].alpha = 1;
		hasChangeAlpha = true;
	}

	const container = containerId
		? canvasContainerMapRef.current.get(containerId)
		: undefined;

	if (imageContainer?.children[0] && hasChangeAlpha) {
		imageContainer.children[0].alpha = 0;
	}

	return canvasApp.renderer.extract.canvas({
		target: container ?? canvasApp.stage,
		frame: new PIXI.Rectangle(
			selectRect.min_x,
			selectRect.min_y,
			selectRect.max_x - selectRect.min_x,
			selectRect.max_y - selectRect.min_y,
		),
	});
};

export const renderRenderToPngAction = async (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	imageContainerKey: string,
	selectRect: ElementRect,
	containerId: string | undefined,
): Promise<ArrayBuffer | undefined> => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	const container = containerId
		? canvasContainerMapRef.current.get(containerId)
		: undefined;

	const imageContainer = canvasContainerMapRef.current.get(imageContainerKey);
	let hasChangeAlpha = false;
	if (imageContainer?.children[0] && imageContainer.children[0].alpha === 0) {
		imageContainer.children[0].alpha = 1;
		hasChangeAlpha = true;
	}

	const canvas = canvasApp.renderer.extract.canvas({
		target: container ?? canvasApp.stage,
		frame: new PIXI.Rectangle(
			selectRect.min_x,
			selectRect.min_y,
			selectRect.max_x - selectRect.min_x,
			selectRect.max_y - selectRect.min_y,
		),
	});

	if (imageContainer?.children[0] && hasChangeAlpha) {
		imageContainer.children[0].alpha = 0;
	}

	const blob = await canvas.convertToBlob?.({
		type: "image/png",
		quality: 1,
	});

	if (!blob) {
		return;
	}

	return await blob.arrayBuffer();
};

export const renderCanvasRenderAction = (
	canvasAppRef: RefType<Application | undefined>,
) => {
	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	canvasApp.render();
};

export const renderAddImageToContainerAction = async (
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	currentImageTextureRef: RefType<PIXI.Texture | undefined>,
	sharedBufferImageTextureRef: RefType<PIXI.Texture | undefined>,
	baseImageTextureRef: RefType<PIXI.Texture | undefined>,
	containerKey: string,
	imageSrc:
		| string
		| ImageSharedBufferData
		| { type: "base_image_texture" }
		| { type: "shared_buffer_image_texture" },
	hideImageSprite?: boolean,
): Promise<void> => {
	const container = canvasContainerMapRef.current.get(containerKey);
	if (!container) {
		return;
	}

	let texture: PIXI.Texture | undefined;
	if (typeof imageSrc === "object") {
		if ("type" in imageSrc) {
			if (imageSrc.type === "base_image_texture") {
				texture = baseImageTextureRef.current;
				baseImageTextureRef.current = undefined;
			} else if (imageSrc.type === "shared_buffer_image_texture") {
				texture = sharedBufferImageTextureRef.current;
			}
		} else {
			texture = new PIXI.Texture({
				source: new PIXI.BufferImageSource({
					resource: imageSrc.sharedBuffer,
					width: imageSrc.width,
					height: imageSrc.height,
					alphaMode: "no-premultiply-alpha",
				}),
			});
			sharedBufferImageTextureRef.current = texture;
		}
	} else if (typeof imageSrc === "string") {
		texture = await PIXI.Assets.load<PIXI.Texture>({
			src: imageSrc,
			parser: "texture",
		});
	}

	container.removeChildren();

	const image = new PIXI.Sprite(texture);
	image.alpha = hideImageSprite ? 0 : 1;
	container.addChild(image);

	currentImageTextureRef.current = texture;
};

export const renderClearContainerAction = (
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	containerKey: string,
) => {
	const container = canvasContainerMapRef.current.get(containerKey);
	if (!container) {
		return;
	}

	container.removeChildren();
};

export type BlurSprite = {
	spriteContainer: PIXI.Container;
	sprite: PIXI.Sprite;
	spriteBackground: PIXI.Sprite;
	spriteBlurFliter: PIXI.Filter;
	spriteMask: PIXI.Graphics;
};

export const renderCreateBlurSpriteAction = (
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	currentImageTextureRef: RefType<PIXI.Texture | undefined>,
	blurSpriteMapRef: RefType<Map<string, BlurSprite>>,
	blurContainerKey: string,
	blurElementId: string,
) => {
	const container = canvasContainerMapRef.current.get(blurContainerKey);
	if (!container) {
		return;
	}

	const imageTexture = currentImageTextureRef.current;
	if (!imageTexture) {
		return;
	}

	const blurSprite: BlurSprite = {
		spriteContainer: new PIXI.Container(),
		sprite: new PIXI.Sprite(imageTexture),
		spriteBackground: new PIXI.Sprite(PIXI.Texture.WHITE),
		spriteBlurFliter: new PIXI.BlurFilter(),
		spriteMask: new PIXI.Graphics(),
	};

	blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
	blurSprite.sprite.x = blurSprite.spriteBackground.x = 0;
	blurSprite.sprite.y = blurSprite.spriteBackground.y = 0;
	blurSprite.sprite.width = imageTexture.width;
	blurSprite.sprite.height = imageTexture.height;
	blurSprite.spriteBackground.setSize({
		width: imageTexture.width,
		height: imageTexture.height,
	});
	blurSprite.spriteContainer.setMask({
		mask: blurSprite.spriteMask,
	});
	blurSprite.spriteMask.setFillStyle({
		color: "white",
		alpha: 1,
	});
	blurSprite.spriteContainer.addChild(blurSprite.spriteBackground);
	blurSprite.spriteContainer.addChild(blurSprite.sprite);
	blurSprite.spriteContainer.addChild(blurSprite.spriteMask);
	container.addChild(blurSprite.spriteContainer);

	blurSpriteMapRef.current.set(blurElementId, blurSprite);
};

export type BlurSpriteProps = {
	blur: number;
	filterType: string;
	x: number;
	y: number;
	width: number;
	height: number;
	angle: number;
	opacity: number;
	zoom: number;
	strokeWidth: number | undefined;
	eraserAlpha: undefined | number;
	points?: readonly [x: number, y: number][];
};

export const renderUpdateBlurSpriteAction = (
	blurSpriteMapRef: RefType<Map<string, BlurSprite>>,
	blurElementId: string,
	blurProps: BlurSpriteProps,
	updateFilter: boolean,
	windowDevicePixelRatio: number,
) => {
	const blurSprite = blurSpriteMapRef.current.get(blurElementId);
	if (!blurSprite) {
		return;
	}

	if (blurProps.points) {
		// 计算points的边界框中心，用于旋转中心
		let minX = 0;
		let minY = 0;

		for (const point of blurProps.points) {
			minX = Math.min(minX, point[0]);
			minY = Math.min(minY, point[1]);
		}

		minX *= windowDevicePixelRatio;
		minY *= windowDevicePixelRatio;

		const rectMinX = 0 + minX + blurProps.x;
		const rectMinY = 0 + minY + blurProps.y;

		blurSprite.spriteMask
			.clear()
			.rotateTransform(blurProps.angle)
			.translateTransform(
				rectMinX + blurProps.width * 0.5,
				rectMinY + blurProps.height * 0.5,
			)
			.scaleTransform(blurProps.zoom, blurProps.zoom);
		const baseX = -(blurProps.width * 0.5 + minX);
		const baseY = -(blurProps.height * 0.5 + minY);
		blurSprite.spriteMask.moveTo(
			blurProps.points[0][0] * windowDevicePixelRatio + baseX,
			blurProps.points[0][1] * windowDevicePixelRatio + baseY,
		);
		for (const point of blurProps.points) {
			blurSprite.spriteMask.lineTo(
				point[0] * windowDevicePixelRatio + baseX,
				point[1] * windowDevicePixelRatio + baseY,
			);
		}
		blurSprite.spriteMask.stroke({
			width:
				(blurProps.strokeWidth ?? 0) *
				9 *
				windowDevicePixelRatio *
				blurProps.zoom,
			join: "round",
			color: "red",
		});
	} else {
		blurSprite.spriteMask
			.clear()
			.rotateTransform(blurProps.angle)
			.translateTransform(
				blurProps.x + blurProps.width * 0.5,
				blurProps.y + blurProps.height * 0.5,
			)
			.scaleTransform(blurProps.zoom, blurProps.zoom)
			.rect(
				-blurProps.width * 0.5,
				-blurProps.height * 0.5,
				blurProps.width,
				blurProps.height,
			)
			.fill();
	}

	blurSprite.spriteContainer.alpha =
		blurProps.eraserAlpha ?? blurProps.opacity / 100;

	if (updateFilter) {
		if (blurProps.filterType === "pixelate") {
			const size = Math.max(1, (blurProps.blur / 100) * 12);
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.PixelateFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.PixelateFilter(size);
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.size = size;
			}
		} else if (blurProps.filterType === "ascii") {
			const size = Math.max(2, (blurProps.blur / 100) * 20);
			if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.AsciiFilter)) {
				blurSprite.spriteBlurFliter = new PIXIFilters.AsciiFilter({
					size,
				});

				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.size = size;
			}
		} else if (blurProps.filterType === "crossHatch") {
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.CrossHatchFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.CrossHatchFilter();
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			}
		} else if (blurProps.filterType === "crt") {
			const lineWidth = Math.max(1, (blurProps.blur / 100) * 5);
			if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.CRTFilter)) {
				blurSprite.spriteBlurFliter = new PIXIFilters.CRTFilter({
					lineWidth,
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.lineWidth = lineWidth;
			}
		} else if (blurProps.filterType === "dot") {
			const scale = Math.max(0.1, blurProps.blur / 100);
			if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.DotFilter)) {
				blurSprite.spriteBlurFliter = new PIXIFilters.DotFilter({
					scale,
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.scale = scale;
			}
		} else if (blurProps.filterType === "emboss") {
			const strength = Math.max(1, (blurProps.blur / 100) * 20);
			if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.EmbossFilter)) {
				blurSprite.spriteBlurFliter = new PIXIFilters.EmbossFilter(strength);
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.strength = strength;
			}
		} else if (blurProps.filterType === "grayscale") {
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.GrayscaleFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.GrayscaleFilter();
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			}
		} else if (blurProps.filterType === "kawaseBlur") {
			const strength = Math.max(1, (blurProps.blur / 100) * 32);
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.KawaseBlurFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.KawaseBlurFilter({
					strength,
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.strength = strength;
			}
		} else if (blurProps.filterType === "motionBlur") {
			const kernelSize = Math.max(1, (blurProps.blur / 100) * 25);
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.MotionBlurFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.MotionBlurFilter({
					kernelSize,
					velocity: {
						x: 42,
						y: 42,
					},
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.kernelSize = kernelSize;
			}
		} else if (blurProps.filterType === "rgbSplit") {
			const offset = (blurProps.blur / 100) * 12;
			if (
				!(blurSprite.spriteBlurFliter instanceof PIXIFilters.RGBSplitFilter)
			) {
				blurSprite.spriteBlurFliter = new PIXIFilters.RGBSplitFilter({
					red: { x: offset, y: offset },
					green: { x: 0, y: 0 },
					blue: { x: -offset, y: -offset },
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.red = { x: offset, y: offset };
				blurSprite.spriteBlurFliter.green = { x: 0, y: 0 };
				blurSprite.spriteBlurFliter.blue = { x: -offset, y: -offset };
			}
		} else if (blurProps.filterType === "noise") {
			const noise = blurProps.blur / 100;
			if (!(blurSprite.spriteBlurFliter instanceof PIXI.NoiseFilter)) {
				blurSprite.spriteBlurFliter = new PIXI.NoiseFilter({ noise });
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.noise = noise;
			}
		} else if (blurProps.filterType === "negative") {
			if (!(blurSprite.spriteBlurFliter instanceof PIXI.ColorMatrixFilter)) {
				const negativeFilter = new PIXI.ColorMatrixFilter();
				negativeFilter.negative(false);
				blurSprite.spriteBlurFliter = negativeFilter;
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			}
		} else {
			const strength = Math.max(1, (blurProps.blur / 100) * 42);
			if (!(blurSprite.spriteBlurFliter instanceof PIXI.BlurFilter)) {
				blurSprite.spriteBlurFliter = new PIXI.BlurFilter({
					strength,
				});
				blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
			} else {
				blurSprite.spriteBlurFliter.strength = strength;
			}
		}
	}
};

export const renderDeleteBlurSpriteAction = (
	blurSpriteMapRef: RefType<Map<string, BlurSprite>>,
	blurElementId: string,
) => {
	const blurSprite = blurSpriteMapRef.current.get(blurElementId);
	if (!blurSprite) {
		return;
	}

	blurSprite.sprite.destroy();
	blurSprite.spriteBackground.destroy();
	blurSprite.spriteContainer.destroy();
	blurSprite.spriteBlurFliter.destroy();
	blurSprite.spriteMask.destroy();
	blurSpriteMapRef.current.delete(blurElementId);
};

export type HighlightProps = {
	selectRectParams: SelectRectParams;
};

export type HighlightElementProps = {
	x: number;
	y: number;
	width: number;
	height: number;
	angle: number;
	opacity: number;
	zoom: number;
	strokeColor: string;
	strokeWidth: number;
	backgroundColor: string;
	maskColor: string;
	maskOpacity: number;
	borderType: string;
	shapeType: string;
	eraserAlpha: number | undefined;
};

export type HighlightElement = {
	props: HighlightElementProps;
	strokeGraphics: PIXI.Graphics;
	backgroundGraphics: PIXI.Graphics;
	backgroundMaskGraphics: PIXI.Graphics;
};

const drawShapeHighlightElementGraphicsAction = (
	graphics: PIXI.Graphics,
	highlightProps: HighlightElementProps,
) => {
	const halfWidth = highlightProps.width * 0.5;
	const halfHeight = highlightProps.height * 0.5;
	graphics
		.rotateTransform(highlightProps.angle)
		.translateTransform(
			highlightProps.x + halfWidth,
			highlightProps.y + halfHeight,
		)
		.scaleTransform(highlightProps.zoom, highlightProps.zoom);
	if (highlightProps.shapeType === "rect") {
		graphics.rect(
			-halfWidth,
			-halfHeight,
			highlightProps.width,
			highlightProps.height,
		);
	} else {
		const radiusX = highlightProps.width * 0.5;
		const radiusY = highlightProps.height * 0.5;
		graphics.ellipse(0, 0, radiusX, radiusY);
	}
};

/**
 * 更新指定 highlight 元素的 props
 */
export const renderUpdateHighlightElementPropsAction = (
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	highlightElementMapRef: RefType<Map<string, HighlightElement>>,
	highlightContainerKey: string,
	highlightElementId: string,
	highlightProps: HighlightElementProps | undefined,
	windowDevicePixelRatio: number,
) => {
	let highlightElement = highlightElementMapRef.current.get(highlightElementId);
	if (!highlightProps) {
		highlightElementMapRef.current.delete(highlightElementId);

		if (highlightElement) {
			highlightElement.strokeGraphics.destroy();
			highlightElement.backgroundGraphics.destroy();
			highlightElement.backgroundMaskGraphics.destroy();
		}

		return;
	}

	const container = canvasContainerMapRef.current.get(highlightContainerKey);
	if (!container) {
		return;
	}

	// 判断是否创建 highlight 的 graphics
	let highlightContainer = container.children[0] as PIXI.Container | undefined; // 渲染 highlight 的背景
	let highlightBackgroundGraphics = container.children[1]
		?.children?.[0] as PIXI.Graphics; // 渲染 highlight 的背景遮罩
	let highlightStrokeContainer = container.children[1]
		?.children?.[1] as PIXI.Container; // 渲染 highlight 的描边
	let highlightBackgroundMaskContainer = container
		.children[2] as PIXI.Container; // 渲染 highlight 的背景遮罩
	if (!highlightContainer) {
		highlightContainer = new PIXI.Container();

		const highlightMaskContentContainer = new PIXI.Container();
		highlightBackgroundGraphics = new PIXI.Graphics();
		highlightStrokeContainer = new PIXI.Container();
		highlightMaskContentContainer.addChild(highlightBackgroundGraphics);
		highlightMaskContentContainer.addChild(highlightStrokeContainer);

		highlightBackgroundMaskContainer = new PIXI.Container();

		highlightMaskContentContainer.setMask({
			mask: highlightBackgroundMaskContainer,
			inverse: true,
		});

		container.addChild(highlightContainer);
		container.addChild(highlightMaskContentContainer);
		container.addChild(highlightBackgroundMaskContainer);
	}

	if (!highlightElement) {
		highlightElement = {
			props: highlightProps,
			strokeGraphics: new PIXI.Graphics(),
			backgroundGraphics: new PIXI.Graphics(),
			backgroundMaskGraphics: new PIXI.Graphics(),
		};

		highlightElement.backgroundGraphics.blendMode = "multiply";

		highlightContainer.addChild(highlightElement.backgroundGraphics);
		highlightStrokeContainer.addChild(highlightElement.strokeGraphics);
		highlightBackgroundMaskContainer.addChild(
			highlightElement.backgroundMaskGraphics,
		);
	}

	highlightElement.props = highlightProps;
	highlightElementMapRef.current.set(highlightElementId, highlightElement);

	const alpha = highlightProps.eraserAlpha ?? highlightProps.opacity / 100;

	highlightElement.strokeGraphics.clear();
	highlightElement.backgroundGraphics.clear();
	highlightElement.backgroundMaskGraphics.clear();

	if (highlightProps.borderType === "solid") {
		drawShapeHighlightElementGraphicsAction(
			highlightElement.strokeGraphics,
			highlightProps,
		);
		highlightElement.strokeGraphics.stroke({
			color: highlightProps.strokeColor,
			width: highlightProps.strokeWidth * 6 * windowDevicePixelRatio,
		});
		highlightElement.strokeGraphics.alpha = alpha;
	}
	drawShapeHighlightElementGraphicsAction(
		highlightElement.backgroundGraphics,
		highlightProps,
	);
	highlightElement.backgroundGraphics.fill({
		color: highlightProps.backgroundColor,
	});
	highlightElement.backgroundGraphics.alpha = alpha;

	drawShapeHighlightElementGraphicsAction(
		highlightElement.backgroundMaskGraphics,
		highlightProps,
	);
	highlightElement.backgroundMaskGraphics.fill("black");
};

/**
 * 重新渲染 highlight
 */
export const renderUpdateHighlightAction = (
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	highlightElementMapRef: RefType<Map<string, HighlightElement>>,
	highlightContainerKey: string,
	highlightProps: HighlightProps,
) => {
	const container = canvasContainerMapRef.current.get(highlightContainerKey);
	if (!container) {
		return;
	}

	const highlightBackgroundGraphics = container.children[1]?.children?.[0] as
		| PIXI.Graphics
		| undefined; // 渲染 highlight 的背景遮罩
	if (!highlightBackgroundGraphics) {
		return;
	}

	highlightBackgroundGraphics.clear();
	if (highlightElementMapRef.current.size === 0) {
		return;
	}

	const firstHighlightElement = highlightElementMapRef.current
		.values()
		.next().value;

	if (firstHighlightElement) {
		highlightBackgroundGraphics
			.roundRect(
				highlightProps.selectRectParams.rect.min_x,
				highlightProps.selectRectParams.rect.min_y,
				highlightProps.selectRectParams.rect.max_x -
					highlightProps.selectRectParams.rect.min_x,
				highlightProps.selectRectParams.rect.max_y -
					highlightProps.selectRectParams.rect.min_y,
				highlightProps.selectRectParams.radius,
			)
			.fill({
				color: firstHighlightElement.props.maskColor,
			});
		highlightBackgroundGraphics.alpha =
			firstHighlightElement.props.maskOpacity / 100;
	}
};

export type WatermarkProps = {
	selectRectParams: SelectRectParams;
	fontSize: number;
	color: string;
	opacity: number;
	text: string;
	visible: boolean;
};

const watermarkTextRotateAngle = Math.PI * (45 / 180);
const watermarkTextPadding = 32;

const getWatermarkSpriteAlpha = (opacity: number) => {
	return (opacity / 100) * 0.24;
};

export const renderUpdateWatermarkSpriteAction = (
	canvasAppRef: RefType<Application | undefined>,
	canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
	watermarkContainerKey: string,
	lastWatermarkPropsRef: RefType<WatermarkProps>,
	watermarkProps: WatermarkProps,
	textResolution: number,
) => {
	const { selectRectParams: lastSelectRectParams } =
		lastWatermarkPropsRef.current;
	const { selectRectParams } = watermarkProps;

	const container = canvasContainerMapRef.current.get(watermarkContainerKey);
	if (!container) {
		return;
	}

	const canvasApp = canvasAppRef.current;
	if (!canvasApp) {
		return;
	}

	container.visible = watermarkProps.visible;

	// 判断是否创建 watermark 的 sprite
	let watermarkSprite = container.children[0] as PIXI.TilingSprite | undefined;
	if (!watermarkSprite) {
		watermarkSprite = new PIXI.TilingSprite();
		container.addChild(watermarkSprite);
		// 重置下 lastWatermarkPropsRef.current 的 text，避免未渲染
		lastWatermarkPropsRef.current.text = "";
		lastWatermarkPropsRef.current.opacity = -1;
	}

	// 判断是否创建 watermark 的 mask
	let watermarkSpriteMask = container.children[1] as PIXI.Graphics | undefined;
	if (!watermarkSpriteMask) {
		watermarkSpriteMask = new PIXI.Graphics();
		container.addChild(watermarkSpriteMask);
		watermarkSprite.setMask({
			mask: watermarkSpriteMask,
		});
	}

	const { rect: selectRect } = selectRectParams;

	if (
		lastWatermarkPropsRef.current.text !== watermarkProps.text ||
		lastWatermarkPropsRef.current.fontSize !== watermarkProps.fontSize ||
		lastWatermarkPropsRef.current.color !== watermarkProps.color
	) {
		const textContainer = new PIXI.Container();
		const textSource = new PIXI.Text({
			text: watermarkProps.text,
			style: {
				fontSize: watermarkProps.fontSize,
				stroke: {
					color: watermarkProps.color,
				},
				fill: watermarkProps.color,
			},
			resolution: textResolution,
		});
		const textWidth = textSource.width;
		const textHeight = textSource.height;
		const rotatedWidth = Math.ceil(
			Math.abs(textWidth * Math.cos(watermarkTextRotateAngle)) +
				Math.abs(textHeight * Math.sin(watermarkTextRotateAngle)),
		);
		const rotatedHeight = Math.ceil(
			Math.abs(textWidth * Math.sin(watermarkTextRotateAngle)) +
				Math.abs(textHeight * Math.cos(watermarkTextRotateAngle)),
		);

		textContainer.addChild(
			new PIXI.Graphics()
				.rect(
					0,
					0,
					rotatedWidth + watermarkTextPadding,
					rotatedHeight + watermarkTextPadding,
				)
				.fill("transparent"),
		);
		textContainer.addChild(textSource);
		textContainer.width = rotatedWidth + watermarkTextPadding;
		textContainer.height = rotatedHeight + watermarkTextPadding;
		textSource.localTransform.rotate(watermarkTextRotateAngle);

		const textTexture = canvasApp.renderer.extract.texture(textContainer);
		watermarkSprite.texture = textTexture;
	}

	if (lastWatermarkPropsRef.current.opacity !== watermarkProps.opacity) {
		watermarkSprite.alpha = getWatermarkSpriteAlpha(watermarkProps.opacity); // 水印保持一定的透明度
	}

	// 比较耗时，做个节流
	if (
		lastSelectRectParams.radius !== selectRectParams.radius ||
		lastSelectRectParams.rect.min_x !== selectRectParams.rect.min_x ||
		lastSelectRectParams.rect.min_y !== selectRectParams.rect.min_y ||
		lastSelectRectParams.rect.max_x !== selectRectParams.rect.max_x ||
		lastSelectRectParams.rect.max_y !== selectRectParams.rect.max_y
	) {
		watermarkSprite.width = selectRect.max_x - selectRect.min_x;
		watermarkSprite.height = selectRect.max_y - selectRect.min_y;
		watermarkSprite.x = selectRect.min_x;
		watermarkSprite.y = selectRect.min_y;

		watermarkSpriteMask
			.clear()
			.roundRect(
				watermarkSprite.x,
				watermarkSprite.y,
				watermarkSprite.width,
				watermarkSprite.height,
				selectRectParams.radius,
			)
			.fill();
	}

	lastWatermarkPropsRef.current = watermarkProps;
	canvasApp.render();
};

export const renderClearContextAction = (
	blurSpriteMapRef: RefType<Map<string, BlurSprite>>,
	highlightElementMapRef: RefType<Map<string, HighlightElement>>,
	lastWatermarkPropsRef: RefType<WatermarkProps>,
) => {
	blurSpriteMapRef.current.clear();
	highlightElementMapRef.current.clear();
	lastWatermarkPropsRef.current = {
		fontSize: 0,
		color: "#000000",
		opacity: 0,
		visible: false,
		text: "",
		selectRectParams: {
			rect: { min_x: 0, min_y: 0, max_x: 0, max_y: 0 },
			radius: 0,
			shadowWidth: 0,
			shadowColor: "#000000",
		},
	};
};
