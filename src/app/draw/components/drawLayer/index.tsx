'use client';

import { useCallback, useContext, useImperativeHandle, useRef } from 'react';
import React from 'react';
import {
    BaseLayerEventActionType,
    withBaseLayer,
    BaseLayerActionType,
    defaultBaseLayerActions,
    BaseLayerContext,
} from '../baseLayer';
import { zIndexs } from '@/utils/zIndex';
import { CaptureHistoryItem } from '@/utils/appStore';
import { INIT_CONTAINER_KEY } from '../baseLayer/actions';
import { convertFileSrc } from '@tauri-apps/api/core';
import { getCaptureHistoryImageAbsPath } from '@/utils/captureHistory';
import { ImageSharedBufferData } from '../../tools';
import { ImageBuffer } from '@/types/commands/screenshot';

export type DrawLayerActionType = BaseLayerActionType & {
    switchCaptureHistory: (item: CaptureHistoryItem | undefined) => Promise<void>;
};

export type DrawLayerProps = {
    actionRef: React.RefObject<DrawLayerActionType | undefined>;
};

export const DRAW_LAYER_BLUR_CONTAINER_KEY = 'draw_layer_blur_container';
export const DRAW_LAYER_HIGHLIGHT_CONTAINER_KEY = 'draw_layer_highlight_container';
export const DRAW_LAYER_WATERMARK_CONTAINER_KEY = 'draw_layer_watermark_container';

const DrawLayerCore: React.FC<DrawLayerProps> = ({ actionRef }) => {
    const {
        createNewCanvasContainer,
        canvasRender,
        addImageToContainer,
        clearContainer,
        createBlurSprite,
        updateBlurSprite,
        updateWatermarkSprite,
        deleteBlurSprite,
        updateHighlightElement,
        updateHighlight,
        clearContext,
    } = useContext(BaseLayerContext);

    const currentCaptureImageSrcRef = useRef<string | ImageSharedBufferData | undefined>(undefined);
    const blurContainerKeyRef = useRef<string | undefined>(undefined);
    const highlightContainerKeyRef = useRef<string | undefined>(undefined);
    const watermarkContainerKeyRef = useRef<string | undefined>(undefined);
    /*
     * 初始化截图
     */
    const onCaptureReady = useCallback<BaseLayerEventActionType['onCaptureReady']>(
        async (
            imageSrc: string | undefined,
            imageBuffer: ImageBuffer | ImageSharedBufferData | undefined,
        ): Promise<void> => {
            // 底图作为单独的层级显示
            const isSharedBuffer = imageBuffer && 'sharedBuffer' in imageBuffer;
            currentCaptureImageSrcRef.current = isSharedBuffer ? imageBuffer : imageSrc;
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
        BaseLayerEventActionType['onCaptureFinish']
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
                const fileUri = convertFileSrc(await getCaptureHistoryImageAbsPath(item.file_name));
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

    useImperativeHandle(
        actionRef,
        () => ({
            ...defaultBaseLayerActions,
            onCaptureReady,
            onCaptureFinish,
            switchCaptureHistory,
            createBlurSprite,
            updateBlurSprite,
            updateWatermarkSprite,
            deleteBlurSprite,
            canvasRender,
            updateHighlightElement,
            updateHighlight,
            clearContext,
        }),
        [
            onCaptureFinish,
            onCaptureReady,
            switchCaptureHistory,
            createBlurSprite,
            updateBlurSprite,
            updateWatermarkSprite,
            deleteBlurSprite,
            canvasRender,
            updateHighlightElement,
            updateHighlight,
            clearContext,
        ],
    );

    return <></>;
};

export const DrawLayer = withBaseLayer(DrawLayerCore, zIndexs.Draw_DrawLayer);
