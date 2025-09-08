import { Application, ApplicationOptions } from 'pixi.js';
import * as PIXI from 'pixi.js';
import { RefWrap } from './workers/renderWorkerTypes';
import { RefObject } from 'react';
import { ElementRect } from '@/commands';
import { SelectRectParams } from '../selectLayer';

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
    sprite: PIXI.Sprite;
    spriteBlurFliter: PIXI.BlurFilter;
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
        sprite: new PIXI.Sprite(imageTexture),
        spriteBlurFliter: new PIXI.BlurFilter(),
        spriteMask: new PIXI.Graphics(),
    };
    blurSprite.sprite.filters = [blurSprite.spriteBlurFliter];
    blurSprite.sprite.x = 0;
    blurSprite.sprite.y = 0;
    blurSprite.sprite.width = imageTexture.width;
    blurSprite.sprite.height = imageTexture.height;
    blurSprite.sprite.setMask({
        mask: blurSprite.spriteMask,
    });
    blurSprite.spriteMask.setFillStyle({
        color: 'white',
        alpha: 1,
    });
    container.addChild(blurSprite.sprite);
    container.addChild(blurSprite.spriteMask);

    blurSpriteMapRef.current.set(blurElementId, blurSprite);
};

export type BlurSpriteProps = {
    blur: number;
    x: number;
    y: number;
    width: number;
    height: number;
    angle: number;
    opacity: number;
    zoom: number;
    eraserAlpha: undefined | number;
};

export const renderUpdateBlurSpriteAction = (
    blurSpriteMapRef: RefType<Map<string, BlurSprite>>,
    blurElementId: string,
    blurProps: BlurSpriteProps,
    updateFilter: boolean,
) => {
    const blurSprite = blurSpriteMapRef.current.get(blurElementId);
    if (!blurSprite) {
        return;
    }

    blurSprite.spriteMask
        .clear()
        .rotateTransform(blurProps.angle)
        .translateTransform(
            blurProps.x + blurProps.width * 0.5,
            blurProps.y + blurProps.height * 0.5,
        )
        .scaleTransform(blurProps.zoom, blurProps.zoom)
        .rect(-blurProps.width * 0.5, -blurProps.height * 0.5, blurProps.width, blurProps.height)
        .fill();
    blurSprite.sprite.alpha = blurProps.eraserAlpha ?? blurProps.opacity / 100;

    if (updateFilter) {
        blurSprite.spriteBlurFliter.strength = Math.max(0, (blurProps.blur / 100) * 32);
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
        watermarkSprite = new PIXI.TilingSprite({
            texture: PIXI.Texture.WHITE,
        });
        container.addChild(watermarkSprite);
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
        watermarkSprite.alpha = (watermarkProps.opacity / 100) * 0.24; // 水印保持一定的透明度
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
