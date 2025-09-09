'use client';

import { theme } from 'antd';
import { zIndexs } from '@/utils/zIndex';
import React, {
    useCallback,
    useContext,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
} from 'react';
import { getCurrentWindow, Window as AppWindow, PhysicalPosition } from '@tauri-apps/api/window';
import { CaptureStep, DrawContext } from '@/app/draw/types';
import { KeyEventKey } from '../drawToolbar/components/keyEventWrap/extra';
import { useCallbackRender, useCallbackRenderSlow } from '@/hooks/useCallbackRender';
import { MousePosition } from '@/utils/mousePosition';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import {
    CaptureEvent,
    CaptureEventParams,
    CaptureEventPublisher,
    CaptureLoadingPublisher,
    CaptureStepPublisher,
    DrawEvent,
    DrawEventPublisher,
    ScreenshotTypePublisher,
} from '../../extra';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { EnableKeyEventPublisher } from '../drawToolbar/components/keyEventWrap/extra';
import { KeyEventWrap } from '../drawToolbar/components/keyEventWrap';
import { debounce } from 'es-toolkit';
import { ScreenshotType } from '@/functions/screenshot';
import {
    AppSettingsActionContext,
    AppSettingsGroup,
    AppSettingsPublisher,
} from '@/app/contextWrap';
import Color, { ColorInstance } from 'color';
import { DrawToolbarStatePublisher } from '../drawToolbar';
import { SelectState } from '../selectLayer/extra';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useMoveCursor } from './extra';
import { getPlatform, supportOffscreenCanvas } from '@/utils';
import { getExcalidrawCanvas } from '@/utils/excalidraw';
import { ImageBuffer } from '@/commands';
import { CaptureHistoryItem } from '@/utils/appStore';
import {
    COLOR_PICKER_PREVIEW_CANVAS_SIZE,
    COLOR_PICKER_PREVIEW_PICKER_SIZE,
    COLOR_PICKER_PREVIEW_SCALE,
} from './renderActions';
import {
    getPreviewImageDataAction,
    initImageDataAction,
    initPreviewCanvasAction,
    pickColorAction,
    putImageDataAction,
    switchCaptureHistoryAction,
    terminateWorkerAction,
} from './actions';
import { writeTextToClipboard } from '@/utils/clipboard';
import { convertFileSrc } from '@tauri-apps/api/core';
import { getCaptureHistoryImageAbsPath } from '@/utils/captureHistory';
import { useStateRef } from '@/hooks/useStateRef';

export enum ColorPickerShowMode {
    Always = 0,
    BeyondSelectRect = 1,
    Never = 2,
}

export const isEnableColorPicker = (
    captureStep: CaptureStep,
    drawState: DrawState,
    captureEvent: CaptureEventParams | undefined,
    toolbarMouseHover: boolean,
) => {
    if (captureEvent?.event !== CaptureEvent.onCaptureLoad) {
        return false;
    }

    if (toolbarMouseHover) {
        return false;
    }

    return (
        captureStep === CaptureStep.Select ||
        (captureStep === CaptureStep.Draw && drawState === DrawState.Idle)
    );
};

export type ColorPickerActionType = {
    getPreviewImageData: () => Promise<ImageData | null>;
    switchCaptureHistory: (item: CaptureHistoryItem | undefined) => Promise<void>;
    pickColor: (mousePosition: MousePosition) => Promise<string>;
    /** 强制启用取色器 */
    setForceEnable: (forceEnable: boolean) => void;
    /** 获取当前颜色 */
    getCurrentColor: () => ColorInstance | undefined;
};

export enum ColorPickerColorFormat {
    RGB = 'rgb',
    HEX = 'hex',
    HSL = 'hsl',
}

const colorPickerColorFormatList = [
    ColorPickerColorFormat.HEX,
    ColorPickerColorFormat.RGB,
    ColorPickerColorFormat.HSL,
];

let decoderWasmModuleArrayBuffer: ArrayBuffer = undefined as unknown as ArrayBuffer;
const getDecoderWasmModuleArrayBuffer = async (): Promise<ArrayBuffer> => {
    if (decoderWasmModuleArrayBuffer) {
        return decoderWasmModuleArrayBuffer;
    }

    decoderWasmModuleArrayBuffer =
        typeof window !== 'undefined'
            ? await fetch(new URL('turbo-png/turbo_png_bg.wasm', import.meta.url)).then((res) =>
                  res.arrayBuffer(),
              )
            : (undefined as unknown as ArrayBuffer);

    return decoderWasmModuleArrayBuffer;
};

const ColorPickerCore: React.FC<{
    onCopyColor?: () => void;
    actionRef: React.Ref<ColorPickerActionType | undefined>;
}> = ({ onCopyColor, actionRef }) => {
    const [getCaptureStep] = useStateSubscriber(CaptureStepPublisher, undefined);
    const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
    const [getDrawToolbarState] = useStateSubscriber(DrawToolbarStatePublisher, undefined);
    const [, setEnableKeyEvent] = useStateSubscriber(EnableKeyEventPublisher, undefined);
    const [getCaptureEvent] = useStateSubscriber(CaptureEventPublisher, undefined);
    const [getScreenshotType] = useStateSubscriber(ScreenshotTypePublisher, undefined);
    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const imageDataReadyRef = useRef(false);

    /** 强制启用取色器 */
    const [, setForceEnable, forceEnableRef] = useStateRef(false);

    const { updateAppSettings } = useContext(AppSettingsActionContext);

    const { token } = theme.useToken();

    const { captureBoundingBoxInfoRef, selectLayerActionRef } = useContext(DrawContext);

    const updateOpacity = useCallback(() => {
        if (!colorPickerRef.current) {
            return;
        }

        const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;
        if (!captureBoundingBoxInfo) {
            return;
        }

        if (!imageDataReadyRef.current) {
            return;
        }

        let opacity = '1';

        if (enableRef.current || forceEnableRef.current) {
            const mouseX = pickerPositionRef.current.mouseX;
            const mouseY = pickerPositionRef.current.mouseY;
            if (
                getAppSettings()[AppSettingsGroup.Screenshot].colorPickerShowMode ===
                ColorPickerShowMode.BeyondSelectRect
            ) {
                const selectRect = selectLayerActionRef.current?.getSelectRect();
                if (selectRect) {
                    const tolerance = token.marginXXS;

                    if (
                        mouseX > selectRect.min_x - tolerance &&
                        mouseX < selectRect.max_x + tolerance &&
                        mouseY > selectRect.min_y - tolerance &&
                        mouseY < selectRect.max_y + tolerance
                    ) {
                        opacity = '1';
                    } else {
                        opacity = '0';
                    }
                } else {
                    opacity = '0';
                }
            } else if (
                getAppSettings()[AppSettingsGroup.Screenshot].colorPickerShowMode ===
                ColorPickerShowMode.Never
            ) {
                opacity = '0';
            } else {
                opacity = '1';
            }

            if (opacity === '1') {
                // 获取选区的状态，如果是未选定的状态，加个透明度
                const selectState = selectLayerActionRef.current?.getSelectState();
                if (selectState === SelectState.Manual || selectState === SelectState.Drag) {
                    opacity = '0.5';
                } else if (selectState === SelectState.Auto && colorPickerRef.current) {
                    // 这时是自动选区，那就根据是否在边缘判断
                    // 一般都是从左上到右下，所以只判断右下边缘即可
                    const maxX =
                        captureBoundingBoxInfo.width -
                        colorPickerRef.current!.clientWidth * window.devicePixelRatio;
                    const maxY =
                        captureBoundingBoxInfo.height -
                        colorPickerRef.current!.clientHeight * window.devicePixelRatio;
                    if (mouseX > maxX || mouseY > maxY) {
                        opacity = '0.5';
                    }
                }
            }
        } else {
            opacity = '0';
        }

        colorPickerRef.current.style.opacity = opacity;
    }, [
        captureBoundingBoxInfoRef,
        forceEnableRef,
        getAppSettings,
        selectLayerActionRef,
        token.marginXXS,
    ]);

    const appWindowRef = useRef<AppWindow | undefined>(undefined);
    useEffect(() => {
        appWindowRef.current = getCurrentWindow();
    }, []);

    const colorPickerRef = useRef<HTMLDivElement>(null);
    const previewCanvasRef = useRef<HTMLCanvasElement>(null);
    const previewOffscreenCanvasRef = useRef<OffscreenCanvas>(null);
    const previewCanvasCtxRef = useRef<CanvasRenderingContext2D | null>(null);
    const decoderWasmModuleArrayBufferRef = useRef<ArrayBuffer | null>(null);
    const pickerPositionRef = useRef<MousePosition>(new MousePosition(0, 0));
    const updatePickerPosition = useCallback((x: number, y: number) => {
        pickerPositionRef.current.mouseX = x;
        pickerPositionRef.current.mouseY = y;

        if (pickerPositionElementRef.current) {
            pickerPositionElementRef.current.textContent = `X: ${x} Y: ${y}`;
        }
    }, []);
    const renderWorker = useMemo(() => {
        if (supportOffscreenCanvas()) {
            return new Worker(new URL('./workers/renderWorker.ts', import.meta.url));
        }

        return undefined;
    }, []);
    useEffect(() => {
        return () => {
            terminateWorkerAction();
            renderWorker?.terminate();
        };
    }, [renderWorker]);

    const enableRef = useRef(false);
    const onEnableChange = useCallback(
        (enable: boolean) => {
            enableRef.current = enable;

            updateOpacity();
        },
        [updateOpacity],
    );
    const updateEnable = useCallback(() => {
        const enable =
            getScreenshotType() !== ScreenshotType.TopWindow &&
            isEnableColorPicker(
                getCaptureStep(),
                getDrawState(),
                getCaptureEvent(),
                getDrawToolbarState().mouseHover,
            );
        if (enableRef.current === enable) {
            return;
        }

        onEnableChange(enable);
    }, [
        getCaptureEvent,
        getCaptureStep,
        getDrawState,
        getScreenshotType,
        getDrawToolbarState,
        onEnableChange,
    ]);
    const updateEnableDebounce = useMemo(() => debounce(updateEnable, 17), [updateEnable]);
    useStateSubscriber(CaptureStepPublisher, updateEnableDebounce);
    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState) => {
                if (colorPickerRef.current) {
                    // 直接隐藏取色器，防止滚动截图干扰
                    if (drawState === DrawState.ScrollScreenshot) {
                        colorPickerRef.current.style.scale = '0';
                    } else {
                        colorPickerRef.current.style.scale = '1';
                    }
                }

                updateEnableDebounce();
            },
            [updateEnableDebounce],
        ),
    );
    useStateSubscriber(CaptureEventPublisher, updateEnableDebounce);
    useStateSubscriber(ScreenshotTypePublisher, updateEnableDebounce);
    useStateSubscriber(DrawToolbarStatePublisher, updateEnableDebounce);
    const [, setDrawEvent] = useStateSubscriber(DrawEventPublisher, undefined);

    const previewImageDataRef = useRef<ImageData | null>(null);
    const captureHistoryImageDataRef = useRef<ImageData | undefined>(undefined);
    const getPreviewImageData = useCallback(async () => {
        return await getPreviewImageDataAction(
            renderWorker,
            previewImageDataRef,
            captureHistoryImageDataRef,
        );
    }, [renderWorker]);

    const colorRef = useRef({
        red: 0,
        green: 0,
        blue: 0,
    });
    // usestate 的性能太低了，直接用 ref 更新
    const colorElementRef = useRef<HTMLDivElement>(null);
    const previewColorElementRef = useRef<HTMLDivElement>(null);

    const currentColorRef = useRef<ColorInstance | undefined>(undefined);

    const getFormatColor = useCallback(
        (red: number, green: number, blue: number) => {
            const currentColor = new Color({
                r: red,
                g: green,
                b: blue,
            });
            currentColorRef.current = currentColor;
            setDrawEvent({
                event: DrawEvent.ColorPickerColorChange,
                params: {
                    color: currentColor,
                },
            });
            setDrawEvent(undefined);

            const colorFormatIndex =
                colorPickerColorFormatList[
                    getAppSettings()[AppSettingsGroup.Cache].colorPickerColorFormatIndex
                ] ?? ColorPickerColorFormat.HEX;

            switch (colorFormatIndex) {
                case ColorPickerColorFormat.HEX:
                    return currentColor.hex().toString();
                case ColorPickerColorFormat.HSL:
                    const hsl = currentColor.hsl();
                    return `hsl(${hsl.hue().toFixed(1)}, ${hsl.saturationl().toFixed(1)}%, ${hsl.lightness().toFixed(1)}%)`;
                case ColorPickerColorFormat.RGB:
                default:
                    return currentColor.rgb().string();
            }
        },
        [getAppSettings, setDrawEvent],
    );
    const updateColor = useCallback(
        (red: number, green: number, blue: number) => {
            colorRef.current.red = red;
            colorRef.current.green = green;
            colorRef.current.blue = blue;
            if (!colorElementRef.current || !previewColorElementRef.current) {
                return;
            }

            colorElementRef.current.style.backgroundColor = `rgb(${red}, ${green}, ${blue})`;
            colorElementRef.current.style.color =
                red > 128 || green > 128 || blue > 128
                    ? 'rgba(0,0,0,0.88)'
                    : 'rgba(255,255,255,0.85)';
            colorElementRef.current.textContent = getFormatColor(red, green, blue);
            previewColorElementRef.current.style.boxShadow = `0 0 0 1px ${colorElementRef.current.style.color}`;
        },
        [getFormatColor],
    );

    const pickerPositionElementRef = useRef<HTMLDivElement>(null);

    const { isDisableMouseMove, enableMouseMove, disableMouseMove } = useMoveCursor();
    const updateImageDataPutImage = useCallback(
        async (x: number, y: number, baseIndex: number) => {
            const color = await putImageDataAction(
                renderWorker,
                previewCanvasCtxRef,
                previewImageDataRef,
                captureHistoryImageDataRef,
                x,
                y,
                baseIndex,
                getAppSettings()[AppSettingsGroup.Screenshot].colorPickerCenterAuxiliaryLineColor,
            );
            // 更新颜色
            updateColor(color.color[0], color.color[1], color.color[2]);
        },
        [getAppSettings, renderWorker, updateColor],
    );

    const convertPositionToImageDataIndex = useCallback(
        (mouseX: number, mouseY: number) => {
            const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;
            if (!captureBoundingBoxInfo) {
                return 0;
            }

            return (mouseY * captureBoundingBoxInfo.width + mouseX) * 4;
        },
        [captureBoundingBoxInfoRef],
    );

    const updateImageDataPutImageRender = useCallbackRender(updateImageDataPutImage);
    const updateImageData = useCallback(
        async (mouseX: number, mouseY: number, physicalX?: number, physicalY?: number) => {
            const captureBoundingBoxInfo = captureBoundingBoxInfoRef.current;
            if (!captureBoundingBoxInfo) {
                return;
            }

            // 恢复鼠标事件触发
            enableMouseMove();

            // 带宽限制，暂时不从系统获取 mouseposition
            // let [mouseX, mouseY] = await getMousePosition();
            // mouseX = Math.min(
            //     Math.max(0, mouseX - imageBufferRef.current!.monitorX),
            //     imageBufferRef.current!.monitorWidth,
            // );
            // mouseY = Math.min(
            //     Math.max(0, mouseY - imageBufferRef.current!.monitorY),
            //     imageBufferRef.current!.monitorHeight,
            // );

            mouseX = Math.min(
                Math.max(0, physicalX ?? Math.floor(mouseX * window.devicePixelRatio)),
                captureBoundingBoxInfo.width - 1,
            );
            mouseY = Math.min(
                Math.max(0, physicalY ?? Math.floor(mouseY * window.devicePixelRatio)),
                captureBoundingBoxInfo.height - 1,
            );

            const halfPickerSize = Math.floor(COLOR_PICKER_PREVIEW_PICKER_SIZE / 2);
            // 将数据绘制到预览画布
            updatePickerPosition(mouseX, mouseY);

            const baseIndex = convertPositionToImageDataIndex(mouseX, mouseY);

            // 计算和绘制错开 1 帧率
            updateImageDataPutImageRender(
                mouseX - halfPickerSize,
                mouseY - halfPickerSize,
                baseIndex,
            );
        },
        [
            captureBoundingBoxInfoRef,
            convertPositionToImageDataIndex,
            enableMouseMove,
            updateImageDataPutImageRender,
            updatePickerPosition,
        ],
    );
    const updateImageRender = useCallbackRenderSlow(updateImageData);

    const updateTransform = useCallback(
        (mouseX: number, mouseY: number) => {
            const colorPickerElement = colorPickerRef.current;
            if (!colorPickerElement) {
                return;
            }

            const colorPickerWidth = colorPickerElement.clientWidth;
            const colorPickerHeight = colorPickerElement.clientHeight;

            const canvasWidth = document.body.clientWidth;
            const canvasHeight = document.body.clientHeight;

            const maxTop = canvasHeight - colorPickerHeight;
            const maxLeft = canvasWidth - colorPickerWidth;

            const colorPickerLeft = Math.min(Math.max(mouseX, 0), maxLeft);
            const colorPickerTop = Math.min(Math.max(mouseY, 0), maxTop);

            colorPickerElement.style.transform = `translate(${colorPickerLeft}px, ${colorPickerTop}px)`;

            updateOpacity();
        },
        [updateOpacity],
    );
    const updateTransformRender = useCallbackRender(updateTransform);
    const update = useCallback(
        (mouseX: number, mouseY: number, physicalX?: number, physicalY?: number) => {
            updateTransformRender(mouseX, mouseY);
            updateImageRender(mouseX, mouseY, physicalX, physicalY);
        },
        [updateImageRender, updateTransformRender],
    );

    const initedPreviewCanvasRef = useRef(false);
    const initPreviewCanvas = useCallback(async () => {
        if (initedPreviewCanvasRef.current) {
            return;
        }

        initedPreviewCanvasRef.current = true;

        const previewCanvas = previewCanvasRef.current;
        if (!previewCanvas) {
            return;
        }

        if (supportOffscreenCanvas() && !previewOffscreenCanvasRef.current) {
            previewOffscreenCanvasRef.current = previewCanvas.transferControlToOffscreen();
        }

        await initPreviewCanvasAction(
            renderWorker,
            previewCanvasRef,
            previewOffscreenCanvasRef,
            previewCanvasCtxRef,
            decoderWasmModuleArrayBufferRef,
            await getDecoderWasmModuleArrayBuffer(),
            previewOffscreenCanvasRef.current ? [previewOffscreenCanvasRef.current] : undefined,
        );
    }, [renderWorker]);

    const refreshMouseMove = useCallback(() => {
        update(
            Math.floor(pickerPositionRef.current.mouseX / window.devicePixelRatio),
            Math.floor(pickerPositionRef.current.mouseY / window.devicePixelRatio),
            pickerPositionRef.current.mouseX,
            pickerPositionRef.current.mouseY,
        );
    }, [update]);

    const initImageData = useCallback(
        async (imageBuffer: ImageBuffer) => {
            await initImageDataAction(
                renderWorker,
                previewCanvasRef,
                previewImageDataRef,
                decoderWasmModuleArrayBufferRef,
                imageBuffer,
            );
            imageDataReadyRef.current = true;
            refreshMouseMove();
        },
        [renderWorker, refreshMouseMove],
    );

    const onCaptureImageBufferReady = useCallback(
        async (imageBuffer: ImageBuffer) => {
            await initImageData(imageBuffer);
        },
        [initImageData],
    );
    const onCaptureLoad = useCallback(
        (captureLoading: boolean) => {
            setEnableKeyEvent(!captureLoading);
        },
        [setEnableKeyEvent],
    );
    useStateSubscriber(CaptureLoadingPublisher, onCaptureLoad);

    useStateSubscriber(
        CaptureEventPublisher,
        useCallback(
            (captureEvent: CaptureEventParams | undefined) => {
                if (captureEvent?.event === CaptureEvent.onCaptureFinish) {
                    imageDataReadyRef.current = false;
                } else if (captureEvent?.event === CaptureEvent.onCaptureImageBufferReady) {
                    onCaptureImageBufferReady(captureEvent.params.imageBuffer);
                }
            },
            [onCaptureImageBufferReady],
        ),
    );

    useEffect(() => {
        const handleMouseMove = (e: MouseEvent) => {
            if (isDisableMouseMove()) {
                return;
            }

            update(e.clientX, e.clientY);
        };

        document.addEventListener('mousemove', handleMouseMove);

        return () => {
            document.removeEventListener('mousemove', handleMouseMove);
        };
    }, [enableRef, isDisableMouseMove, update]);

    const moveCursor = useCallback(
        (offsetX: number, offsetY: number) => {
            const appWindow = appWindowRef.current;
            if (!appWindow) {
                return;
            }

            let mouseX = pickerPositionRef.current.mouseX + offsetX;
            let mouseY = pickerPositionRef.current.mouseY + offsetY;

            if (mouseX < 0) {
                mouseX = 0;
            } else if (mouseX > captureBoundingBoxInfoRef.current!.width) {
                mouseX = captureBoundingBoxInfoRef.current!.width;
            }

            if (mouseY < 0) {
                mouseY = 0;
            } else if (mouseY > captureBoundingBoxInfoRef.current!.height) {
                mouseY = captureBoundingBoxInfoRef.current!.height;
            }

            disableMouseMove();
            appWindow.setCursorPosition(new PhysicalPosition(mouseX, mouseY));
            setDrawEvent({
                event: DrawEvent.MoveCursor,
                params: {
                    x: mouseX,
                    y: mouseY,
                },
            });
            setDrawEvent(undefined);

            // 在 macOS 下，鼠标移动不会触发 mousemove 事件，给 excalidraw 发送一个 mousemove 事件
            if (getPlatform() === 'macos') {
                const canvas = getExcalidrawCanvas();
                canvas?.dispatchEvent(
                    new PointerEvent('pointermove', {
                        clientX: Math.round(mouseX / window.devicePixelRatio),
                        clientY: Math.round(mouseY / window.devicePixelRatio),
                        bubbles: true,
                        cancelable: true,
                    }),
                );
            }

            update(
                mouseX / window.devicePixelRatio,
                mouseY / window.devicePixelRatio,
                mouseX,
                mouseY,
            );
        },
        [captureBoundingBoxInfoRef, disableMouseMove, setDrawEvent, update],
    );

    const switchCaptureHistory = useCallback(
        async (item: CaptureHistoryItem | undefined) => {
            const fileUri = item
                ? convertFileSrc(await getCaptureHistoryImageAbsPath(item.file_name))
                : undefined;
            await switchCaptureHistoryAction(
                renderWorker,
                decoderWasmModuleArrayBufferRef,
                captureHistoryImageDataRef,
                fileUri,
            );
            refreshMouseMove();
        },
        [renderWorker, refreshMouseMove],
    );

    const pickColor = useCallback(
        async (mousePosition: MousePosition): Promise<string> => {
            const baseIndex = convertPositionToImageDataIndex(
                Math.round(mousePosition.mouseX * window.devicePixelRatio),
                Math.round(mousePosition.mouseY * window.devicePixelRatio),
            );
            const color = await pickColorAction(
                renderWorker,
                captureHistoryImageDataRef,
                previewImageDataRef,
                baseIndex,
            );

            return Color({
                r: color.color[0],
                g: color.color[1],
                b: color.color[2],
            })
                .hex()
                .toString();
        },
        [convertPositionToImageDataIndex, renderWorker],
    );

    useImperativeHandle(
        actionRef,
        () => ({
            getPreviewImageData,
            switchCaptureHistory,
            pickColor,
            setForceEnable,
            getCurrentColor: () => currentColorRef.current,
        }),
        [getPreviewImageData, switchCaptureHistory, pickColor, setForceEnable, currentColorRef],
    );

    useEffect(() => {
        initPreviewCanvas();
    }, [initPreviewCanvas]);

    return (
        <div className="color-picker" ref={colorPickerRef}>
            <KeyEventWrap
                componentKey={KeyEventKey.ColorPickerCopy}
                onKeyDown={() => {
                    if (!enableRef.current) {
                        return;
                    }

                    writeTextToClipboard(
                        getFormatColor(
                            colorRef.current.red,
                            colorRef.current.green,
                            colorRef.current.blue,
                        ),
                    );
                    onCopyColor?.();
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.ColorPickerMoveUp}
                onKeyDown={() => {
                    moveCursor(0, -1);
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.ColorPickerMoveDown}
                onKeyDown={() => {
                    moveCursor(0, 1);
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.ColorPickerMoveLeft}
                onKeyDown={() => {
                    moveCursor(-1, 0);
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.ColorPickerMoveRight}
                onKeyDown={() => {
                    moveCursor(1, 0);
                }}
            >
                <div />
            </KeyEventWrap>
            <KeyEventWrap
                componentKey={KeyEventKey.SwitchColorFormat}
                onKeyUp={() => {
                    if (!enableRef.current) {
                        return;
                    }

                    // 手动框选时，会触发固定宽高，忽略切换颜色格式
                    if (
                        selectLayerActionRef.current?.getSelectState() === SelectState.Manual ||
                        selectLayerActionRef.current?.getSelectState() === SelectState.Drag
                    ) {
                        return;
                    }

                    updateAppSettings(
                        AppSettingsGroup.Cache,
                        {
                            colorPickerColorFormatIndex:
                                (getAppSettings()[AppSettingsGroup.Cache]
                                    .colorPickerColorFormatIndex +
                                    1) %
                                colorPickerColorFormatList.length,
                        },
                        false,
                        true,
                        false,
                        true,
                        false,
                    );

                    updateColor(
                        colorRef.current.red,
                        colorRef.current.green,
                        colorRef.current.blue,
                    );
                }}
            >
                <div />
            </KeyEventWrap>

            <div className="color-picker-container">
                <div className="color-picker-preview">
                    <canvas ref={previewCanvasRef} className="preview-canvas" />
                    <div ref={previewColorElementRef} className="color-picker-preview-border" />
                </div>
            </div>

            <div className="color-picker-content">
                <div className="color-picker-content-text" ref={pickerPositionElementRef}></div>
                <div className="color-picker-content-color" ref={colorElementRef}></div>
            </div>
            <style jsx>
                {`
                    .color-picker {
                        user-select: none;
                        position: fixed;
                        top: 0;
                        left: 0;
                        background-color: ${token.colorBgContainer};
                        border-radius: ${token.borderRadius}px;
                        box-shadow: ${token.boxShadowSecondary};
                        z-index: ${zIndexs.Draw_ColorPicker};
                        pointer-events: none;
                        display: flex;
                        align-items: center;
                        justify-content: center;
                        opacity: 0;
                        color: ${token.colorText};
                        display: flex;
                        flex-direction: column;
                        padding: ${token.paddingXXS}px;
                        transition: opacity ${token.motionDurationFast} ${token.motionEaseInOut};
                    }

                    .color-picker-container {
                        width: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE}px;
                        height: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE}px;
                        border-radius: ${token.borderRadius}px;
                        overflow: hidden;
                    }

                    .color-picker-preview {
                    }

                    .color-picker-preview-border {
                        position: absolute;
                        width: ${1 * COLOR_PICKER_PREVIEW_SCALE}px;
                        height: ${1 * COLOR_PICKER_PREVIEW_SCALE}px;
                        left: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE / 2 - 2}px;
                        top: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE / 2 - 2}px;
                        background-color: transparent;
                        border-radius: ${token.borderRadiusXS}px;
                    }

                    .preview-canvas {
                        width: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE}px;
                        height: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE}px;
                        image-rendering: pixelated;
                    }

                    .color-picker-content {
                        display: flex;
                        flex-direction: column;
                        gap: ${token.marginXXS}px;
                        margin-top: ${token.marginXXS}px;
                        font-size: ${token.fontSizeSM}px;
                        width: ${COLOR_PICKER_PREVIEW_CANVAS_SIZE}px;
                        border-bottom-left-radius: ${token.borderRadius}px;
                        border-bottom-right-radius: ${token.borderRadius}px;
                        overflow: hidden;
                    }

                    .color-picker-content-color {
                        padding: ${token.paddingXXS}px;
                        text-align: center;
                    }

                    .color-picker-content-text {
                        text-align: center;
                    }
                `}
            </style>
        </div>
    );
};

export const ColorPicker = React.memo(withStatePublisher(ColorPickerCore, EnableKeyEventPublisher));
