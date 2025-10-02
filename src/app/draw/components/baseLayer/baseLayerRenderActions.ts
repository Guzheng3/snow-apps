import { Application, ApplicationOptions } from 'pixi.js';
import * as PIXI from 'pixi.js';
import { RefWrap } from './workers/renderWorkerTypes';
import { RefObject } from 'react';
import { ElementRect } from '@/commands';
import { SelectRectParams } from '../selectLayer';
import * as PIXIFilters from 'pixi-filters';

export type RefType<T> = RefWrap<T> | RefObject<T>;

export const renderDisposeCanvasAction = (canvasAppRef: RefType<Application | undefined>) => {
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

    canvasApp.render();
};

export const renderGetImageDataAction = (
    canvasAppRef: RefType<Application | undefined>,
    selectRect: ElementRect,
): ImageData | undefined => {
    const canvasApp = canvasAppRef.current;
    if (!canvasApp) {
        return;
    }

    const pixels = canvasApp.renderer.extract.pixels({
        target: canvasApp.stage,
        frame: new PIXI.Rectangle(
            selectRect.min_x,
            selectRect.min_y,
            selectRect.max_x - selectRect.min_x,
            selectRect.max_y - selectRect.min_y,
        ),
    });

    const res = new ImageData(pixels.pixels as ImageDataArray, pixels.width, pixels.height);

    return res;
};

export const renderRenderToCanvasAction = (
    canvasAppRef: RefType<Application | undefined>,
    selectRect: ElementRect,
): PIXI.ICanvas | undefined => {
    const canvasApp = canvasAppRef.current;
    if (!canvasApp) {
        return;
    }

    return canvasApp.renderer.extract.canvas({
        target: canvasApp.stage,
        frame: new PIXI.Rectangle(
            selectRect.min_x,
            selectRect.min_y,
            selectRect.max_x - selectRect.min_x,
            selectRect.max_y - selectRect.min_y,
        ),
    });
};

export const renderCanvasRenderAction = (canvasAppRef: RefType<Application | undefined>) => {
    const canvasApp = canvasAppRef.current;
    if (!canvasApp) {
        return;
    }

    canvasApp.render();
};

export const renderAddImageToContainerAction = async (
    canvasContainerMapRef: RefType<Map<string, PIXI.Container>>,
    currentImageTextureRef: RefType<PIXI.Texture | undefined>,
    containerKey: string,
    imageSrc: string,
): Promise<void> => {
    const container = canvasContainerMapRef.current.get(containerKey);
    if (!container) {
        return;
    }

    const texture = await PIXI.Assets.load<PIXI.Texture>({
        src: imageSrc,
        parser: 'texture',
    });
    const image = new PIXI.Sprite(texture);
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
        color: 'white',
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
            .translateTransform(rectMinX + blurProps.width * 0.5, rectMinY + blurProps.height * 0.5)
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
            width: blurProps.strokeWidth! * 9 * windowDevicePixelRatio * blurProps.zoom,
            join: 'round',
            color: 'red',
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

    blurSprite.sprite.alpha = blurProps.eraserAlpha ?? blurProps.opacity / 100;

    if (updateFilter) {
        if (blurProps.filterType === 'pixelate') {
            const size = Math.max(1, (blurProps.blur / 100) * 12);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.PixelateFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.PixelateFilter(size);
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.size = size;
            }
        } else if (blurProps.filterType === 'ascii') {
            const size = Math.max(2, (blurProps.blur / 100) * 20);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.AsciiFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.AsciiFilter({
                    size,
                });

                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.size = size;
            }
        } else if (blurProps.filterType === 'crossHatch') {
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.CrossHatchFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.CrossHatchFilter();
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            }
        } else if (blurProps.filterType === 'crt') {
            const lineWidth = Math.max(1, (blurProps.blur / 100) * 5);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.CRTFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.CRTFilter({
                    lineWidth,
                });
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.lineWidth = lineWidth;
            }
        } else if (blurProps.filterType === 'dot') {
            const scale = Math.max(0.1, blurProps.blur / 100);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.DotFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.DotFilter({
                    scale,
                });
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.scale = scale;
            }
        } else if (blurProps.filterType === 'emboss') {
            const strength = Math.max(1, (blurProps.blur / 100) * 20);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.EmbossFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.EmbossFilter(strength);
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.strength = strength;
            }
        } else if (blurProps.filterType === 'grayscale') {
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.GrayscaleFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.GrayscaleFilter();
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            }
        } else if (blurProps.filterType === 'kawaseBlur') {
            const strength = Math.max(1, (blurProps.blur / 100) * 32);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.KawaseBlurFilter)) {
                blurSprite.spriteBlurFliter = new PIXIFilters.KawaseBlurFilter({ strength });
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.strength = strength;
            }
        } else if (blurProps.filterType === 'motionBlur') {
            const kernelSize = Math.max(1, (blurProps.blur / 100) * 25);
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.MotionBlurFilter)) {
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
        } else if (blurProps.filterType === 'rgbSplit') {
            const offset = (blurProps.blur / 100) * 12;
            if (!(blurSprite.spriteBlurFliter instanceof PIXIFilters.RGBSplitFilter)) {
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
        } else if (blurProps.filterType === 'noise') {
            const noise = blurProps.blur / 100;
            if (!(blurSprite.spriteBlurFliter instanceof PIXI.NoiseFilter)) {
                blurSprite.spriteBlurFliter = new PIXI.NoiseFilter({ noise });
                blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
            } else {
                blurSprite.spriteBlurFliter.noise = noise;
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
    const { selectRectParams: lastSelectRectParams } = lastWatermarkPropsRef.current;
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
        lastWatermarkPropsRef.current.text = '';
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
                .fill('transparent'),
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
