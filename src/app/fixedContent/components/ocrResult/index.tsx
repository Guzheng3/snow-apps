import {
    useCallback,
    useContext,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
    useState,
} from 'react';
import { ElementRect } from '@/commands';
import { ocrDetect, OcrDetectResult } from '@/commands/ocr';
import { FormattedMessage, useIntl } from 'react-intl';
import { theme } from 'antd';
import Color from 'color';
import { getCurrentWindow } from '@tauri-apps/api/window';
import { Menu } from '@tauri-apps/api/menu';
import { useHotkeysApp } from '@/hooks/useHotkeysApp';
import { AntdContext } from '@/components/globalLayoutExtra';
import { CaptureBoundingBoxInfo, ElementDraggingPublisher } from '@/app/draw/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { AppSettingsGroup, AppSettingsPublisher } from '@/app/contextWrap';
import { writeTextToClipboard } from '@/utils/clipboard';
import { getPlatformValue } from '@/utils';
import { releaseOcrSession } from '@/functions/ocr';
import { PLUGIN_ID_RAPID_OCR, usePluginService } from '@/components/pluginService';

// 定义角度阈值常量（以度为单位）
const ROTATION_THRESHOLD = 3; // 小于3度的旋转被视为误差，不进行旋转

export type AppOcrResult = {
    result: OcrDetectResult;
    ignoreScale: boolean;
};

export type OcrResultInitDrawCanvasParams = {
    selectRect: ElementRect;
    canvas: HTMLCanvasElement;
    captureBoundingBoxInfo: CaptureBoundingBoxInfo;
    /** 已有的 OCR 结果 */
    ocrResult: AppOcrResult | undefined;
};

export type OcrResultInitImageParams = {
    imageElement: HTMLImageElement;
    monitorScaleFactor: number;
};

export type OcrResultActionType = {
    init: (params: OcrResultInitDrawCanvasParams | OcrResultInitImageParams) => Promise<void>;
    setEnable: (enable: boolean | ((enable: boolean) => boolean)) => void;
    setScale: (scale: number) => void;
    clear: () => void;
    updateOcrTextElements: (ocrResult: OcrDetectResult, ignoreScale?: boolean) => void;
    getOcrResult: () => AppOcrResult | undefined;
    getSelectedText: () => string | undefined;
};

export const covertOcrResultToText = (ocrResult: OcrDetectResult) => {
    return ocrResult.text_blocks.map((block) => block.text).join('\n');
};

export enum OcrDetectAfterAction {
    /** 不执行任何操作 */
    None = 'none',
    /** 复制文本 */
    CopyText = 'copyText',
    /** 复制文本并关闭窗口 */
    CopyTextAndCloseWindow = 'copyTextAndCloseWindow',
}

export const OcrResult: React.FC<{
    zIndex: number;
    actionRef: React.RefObject<OcrResultActionType | undefined>;
    onOcrDetect?: (ocrResult: OcrDetectResult) => void;
    onContextMenu?: (event: React.MouseEvent<HTMLDivElement>) => void;
    onWheel?: (event: React.WheelEvent<HTMLDivElement>) => void;
    enableCopy?: boolean;
    disabled?: boolean;
    onMouseDown?: (event: React.MouseEvent<HTMLDivElement>) => void;
    onMouseMove?: (event: React.MouseEvent<HTMLDivElement>) => void;
    onMouseUp?: (event: React.MouseEvent<HTMLDivElement>) => void;
    style?: React.CSSProperties;
}> = ({
    zIndex,
    actionRef,
    onOcrDetect,
    onContextMenu: onContextMenuProp,
    onWheel,
    enableCopy,
    disabled,
    onMouseDown,
    onMouseMove,
    onMouseUp,
    style,
}) => {
    const intl = useIntl();
    const { token } = theme.useToken();
    const { message } = useContext(AntdContext);

    const containerElementRef = useRef<HTMLDivElement>(null);
    const textContainerElementRef = useRef<HTMLDivElement>(null);
    const textIframeContainerElementWrapRef = useRef<HTMLDivElement>(null);
    const textIframeContainerElementRef = useRef<HTMLIFrameElement>(null);
    const [textContainerContent, setTextContainerContent] = useState('');

    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);

    const currentOcrResultRef = useRef<AppOcrResult | undefined>(undefined);

    const enableRef = useRef<boolean>(false);
    const setEnable = useCallback((enable: boolean | ((enable: boolean) => boolean)) => {
        if (!containerElementRef.current) {
            return;
        }

        if (typeof enable === 'function') {
            enableRef.current = enable(enableRef.current);
        } else {
            enableRef.current = enable;
        }

        if (enableRef.current) {
            containerElementRef.current.style.opacity = '1';
            containerElementRef.current.style.pointerEvents = 'auto';
        } else {
            containerElementRef.current.style.opacity = '0';
            containerElementRef.current.style.pointerEvents = 'none';
        }
    }, []);

    const selectRectRef = useRef<ElementRect>(undefined);
    const monitorScaleFactorRef = useRef<number>(undefined);
    const updateOcrTextElements = useCallback(
        async (ocrResult: OcrDetectResult, ignoreScale?: boolean) => {
            const monitorScaleFactor = monitorScaleFactorRef.current;
            const selectRect = selectRectRef.current;

            if (!selectRect || !monitorScaleFactor) {
                return;
            }

            currentOcrResultRef.current = {
                result: ocrResult,
                ignoreScale: ignoreScale ?? false,
            };

            const transformScale = 1 / monitorScaleFactor;

            const baseX = selectRect.min_x * transformScale;
            const baseY = selectRect.min_y * transformScale;

            const textContainerElement = textContainerElementRef.current;
            const textIframeContainerWrapElement = textIframeContainerElementWrapRef.current;
            if (!textContainerElement || !textIframeContainerWrapElement) {
                return;
            }

            textContainerElement.innerHTML = '';

            textContainerElement.style.left =
                textIframeContainerWrapElement.style.left = `${baseX}px`;
            textContainerElement.style.top =
                textIframeContainerWrapElement.style.top = `${baseY}px`;
            textContainerElement.style.width =
                textIframeContainerWrapElement.style.width = `${(selectRect.max_x - selectRect.min_x) * transformScale}px`;
            textContainerElement.style.height =
                textIframeContainerWrapElement.style.height = `${(selectRect.max_y - selectRect.min_y) * transformScale}px`;

            await Promise.all(
                ocrResult.text_blocks.map(async (block) => {
                    if (isNaN(block.text_score) || block.text_score < 0.3) {
                        return null;
                    }

                    const rectLeftTopX = block.box_points[0].x * transformScale;
                    const rectLeftTopY = block.box_points[0].y * transformScale;
                    const rectRightTopX = block.box_points[1].x * transformScale;
                    const rectRightTopY = block.box_points[1].y * transformScale;
                    const rectRightBottomX = block.box_points[2].x * transformScale;
                    const rectRightBottomY = block.box_points[2].y * transformScale;
                    const rectLeftBottomX = block.box_points[3].x * transformScale;
                    const rectLeftBottomY = block.box_points[3].y * transformScale;

                    // 计算矩形中心点
                    const centerX =
                        (rectLeftTopX + rectRightTopX + rectRightBottomX + rectLeftBottomX) / 4;
                    const centerY =
                        (rectLeftTopY + rectRightTopY + rectRightBottomY + rectLeftBottomY) / 4;

                    // 计算矩形旋转角度 (使用顶边与水平线的夹角)
                    const rotationRad = Math.atan2(
                        rectRightTopY - rectLeftTopY,
                        rectRightTopX - rectLeftTopX,
                    );
                    let rotationDeg = rotationRad * (180 / Math.PI);

                    // 如果旋转角度小于阈值，则视为误差，不进行旋转
                    if (Math.abs(rotationDeg) < ROTATION_THRESHOLD) {
                        rotationDeg = 0;
                    }

                    // 计算宽度和高度
                    const width = Math.sqrt(
                        Math.pow(rectRightTopX - rectLeftTopX, 2) +
                            Math.pow(rectRightTopY - rectLeftTopY, 2),
                    );
                    const height = Math.sqrt(
                        Math.pow(rectLeftBottomX - rectLeftTopX, 2) +
                            Math.pow(rectLeftBottomY - rectLeftTopY, 2),
                    );

                    const textElement = document.createElement('div');
                    textElement.innerText = block.text;
                    textElement.style.color = token.colorText;
                    textElement.style.display = 'inline-block';
                    textElement.className = 'ocr-result-text-element';

                    const textWrapElement = document.createElement('div');
                    const textBackgroundElement = document.createElement('div');
                    textBackgroundElement.className = 'ocr-result-text-background-element';
                    textBackgroundElement.style.position = textWrapElement.style.position =
                        'absolute';
                    textBackgroundElement.style.width = textWrapElement.style.width = `${width}px`;
                    textBackgroundElement.style.height =
                        textWrapElement.style.height = `${height}px`;
                    textBackgroundElement.style.transformOrigin =
                        textWrapElement.style.transformOrigin = 'center';

                    textWrapElement.style.display = 'flex';
                    textWrapElement.style.alignItems = 'center';
                    textWrapElement.style.justifyContent = 'center';
                    textWrapElement.style.backgroundColor = 'transparent';
                    textWrapElement.style.zIndex = '1';

                    textBackgroundElement.style.backgroundColor = Color(token.colorBgContainer)
                        .alpha(0.42)
                        .toString();

                    const isVertical = !ignoreScale && height > width * 1.5;
                    if (isVertical) {
                        textWrapElement.style.writingMode = 'vertical-rl';
                    }

                    if (ignoreScale) {
                        textElement.style.whiteSpace = 'normal';
                        textElement.style.fontSize = '16px';
                        textElement.style.wordBreak = 'break-all';
                    } else {
                        textElement.style.fontSize = '12px';
                        textElement.style.whiteSpace = 'nowrap';
                        textWrapElement.style.textAlign = 'center';
                    }

                    textElement.setAttribute('onmousedown', 'event.stopPropagation();');
                    textElement.style.cursor = 'text';

                    textWrapElement.appendChild(textElement);
                    textContainerElement.appendChild(textBackgroundElement);
                    textContainerElement.appendChild(textWrapElement);

                    await new Promise((resolve) => {
                        requestAnimationFrame(() => {
                            let textWidth = textElement.clientWidth;
                            let textHeight = textElement.clientHeight;
                            if (isVertical) {
                                textWidth -= 1;
                            } else {
                                textHeight -= 1;
                            }

                            const scale = Math.min(height / textHeight, width / textWidth);
                            textElement.style.transform = `scale(${scale})`;
                            const leftWidth = Math.max(0, width - textWidth * scale); // 文本的宽度可能小于 OCR 识别的宽度
                            let letterSpaceWidth = 0;
                            if (textElement.innerText.length > 1) {
                                // letterSpace 对于每个字符都生效，行首也要加一个间距，所以 +1
                                const letterSpaceCount = textElement.innerText.length + 1;
                                letterSpaceWidth = leftWidth / letterSpaceCount / scale;
                            }
                            textElement.style.letterSpacing = `${letterSpaceWidth}px`;
                            textElement.style.textIndent = `${letterSpaceWidth}px`;
                            textBackgroundElement.style.transform =
                                textWrapElement.style.transform = `translate(${centerX - width * 0.5}px, ${centerY - height * 0.5}px) rotate(${rotationDeg}deg)`;

                            resolve(undefined);
                        });
                    });
                }),
            );
            setTextContainerContent(
                textContainerElement.innerHTML ? textContainerElement.innerHTML : ' ', // 避免空字符串导致 iframe 内容为空
            );
        },
        [token.colorBgContainer, token.colorText],
    );
    const setScale = useCallback((scale: number) => {
        if (!textContainerElementRef.current || !textIframeContainerElementWrapRef.current) {
            return;
        }

        textContainerElementRef.current.style.transform = `scale(${scale / 100})`;
        textIframeContainerElementWrapRef.current.style.transform = `scale(${scale / 100})`;
    }, []);

    /** 请求 ID，避免 OCR 检测中切换工具后任然触发 OCR 结果 */
    const requestIdRef = useRef<number>(0);
    const { isReady } = usePluginService();
    const initDrawCanvas = useCallback(
        async (params: OcrResultInitDrawCanvasParams) => {
            requestIdRef.current++;
            const currentRequestId = requestIdRef.current;

            const { selectRect, canvas } = params;

            const imageBlob = await new Promise<Blob | null>((resolve) => {
                canvas.toBlob(resolve, 'image/png', 1);
            });

            if (imageBlob && isReady?.(PLUGIN_ID_RAPID_OCR)) {
                monitorScaleFactorRef.current = window.devicePixelRatio;
                const ocrResult = params.ocrResult ?? {
                    result: await ocrDetect(
                        await imageBlob.arrayBuffer(),
                        monitorScaleFactorRef.current,
                        getAppSettings()[AppSettingsGroup.SystemScreenshot].ocrDetectAngle,
                    ).finally(() => {
                        releaseOcrSession();
                    }),
                    ignoreScale: false,
                };

                // 如果请求 ID 不一致，说明 OCR 检测中切换工具了，不进行更新
                if (currentRequestId !== requestIdRef.current) {
                    return;
                }

                selectRectRef.current = selectRect;
                updateOcrTextElements(ocrResult.result, ocrResult.ignoreScale);
                onOcrDetect?.(ocrResult.result);
            }
        },
        [getAppSettings, isReady, onOcrDetect, updateOcrTextElements],
    );

    const initImage = useCallback(
        async (params: OcrResultInitImageParams) => {
            const { imageElement } = params;

            const tempCanvas = document.createElement('canvas');
            tempCanvas.width = imageElement.naturalWidth;
            tempCanvas.height = imageElement.naturalHeight;
            const tempCtx = tempCanvas.getContext('2d');
            if (!tempCtx) {
                return;
            }

            tempCtx.drawImage(imageElement, 0, 0);

            const imageBlob = await new Promise<Blob | null>((resolve) => {
                tempCanvas.toBlob(resolve, 'image/png', 1);
            });

            selectRectRef.current = {
                min_x: 0,
                min_y: 0,
                max_x: imageElement.naturalWidth,
                max_y: imageElement.naturalHeight,
            };
            monitorScaleFactorRef.current = params.monitorScaleFactor;

            if (imageBlob && isReady?.(PLUGIN_ID_RAPID_OCR)) {
                const ocrResult = await ocrDetect(
                    await imageBlob.arrayBuffer(),
                    0,
                    getAppSettings()[AppSettingsGroup.SystemScreenshot].ocrDetectAngle,
                );
                releaseOcrSession();

                updateOcrTextElements(ocrResult);
                onOcrDetect?.(ocrResult);
            }
        },
        [getAppSettings, isReady, onOcrDetect, updateOcrTextElements],
    );

    const selectedTextRef = useRef<string>(undefined);
    const getSelectedText = useCallback(() => {
        return textIframeContainerElementRef.current?.contentWindow
            ?.getSelection()
            ?.toString()
            .trim();
    }, []);

    useImperativeHandle(
        actionRef,
        () => ({
            init: async (params: OcrResultInitDrawCanvasParams | OcrResultInitImageParams) => {
                const hideLoading = message.loading(<FormattedMessage id="draw.ocrLoading" />, 60);

                if ('selectRect' in params) {
                    await initDrawCanvas(params);
                } else if ('imageElement' in params) {
                    await initImage(params);
                }

                hideLoading();
            },
            setEnable,
            setScale,
            clear: () => {
                setTextContainerContent('');
                if (textContainerElementRef.current) {
                    textContainerElementRef.current.innerHTML = '';
                }
            },
            updateOcrTextElements,
            getOcrResult: () => {
                return currentOcrResultRef.current;
            },
            getSelectedText,
        }),
        [
            getSelectedText,
            initDrawCanvas,
            initImage,
            message,
            setEnable,
            setScale,
            updateOcrTextElements,
        ],
    );

    const menuRef = useRef<Menu>(undefined);

    const initMenu = useCallback(async () => {
        if (disabled) {
            return;
        }

        if (menuRef.current) {
            await menuRef.current.close();
            menuRef.current = undefined;
            return;
        }
        const appWindow = getCurrentWindow();
        const menu = await Menu.new({
            id: `${appWindow.label}-ocrResultMenu`,
            items: [
                {
                    id: `${appWindow.label}-copySelectedText`,
                    text: intl.formatMessage({ id: 'draw.copySelectedText' }),
                    action: async () => {
                        writeTextToClipboard(selectedTextRef.current || '');
                    },
                },
            ],
        });
        menuRef.current = menu;
    }, [disabled, intl]);

    useEffect(() => {
        initMenu();

        return () => {
            menuRef.current?.close();
            menuRef.current = undefined;
        };
    }, [initMenu]);

    useHotkeysApp(
        getPlatformValue('Ctrl+A', 'Meta+A'),
        useCallback((event) => {
            if (!enableRef.current) {
                return;
            }

            event.preventDefault();

            const selection = textIframeContainerElementRef.current?.contentWindow?.getSelection();
            const targetElement = textIframeContainerElementRef.current?.contentDocument;
            if (containerElementRef.current && selection && targetElement) {
                textIframeContainerElementRef.current?.focus();
                const range = targetElement.createRange();
                range.selectNodeContents(targetElement.body);
                selection.removeAllRanges();
                selection.addRange(range);
            }
        }, []),
        useMemo(
            () => ({
                keyup: false,
                keydown: true,
                preventDefault: true,
            }),
            [],
        ),
    );

    const onContextMenu = useCallback(() => {
        selectedTextRef.current = getSelectedText();
        if (selectedTextRef.current) {
            menuRef.current?.popup();
            return;
        }

        onContextMenuProp?.({
            preventDefault: () => {},
            stopPropagation: () => {},
            clientX: 0,
            clientY: 0,
        } as React.MouseEvent<HTMLDivElement>);
    }, [getSelectedText, onContextMenuProp]);

    const onDoubleClick = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
        // 阻止截图双击复制和固定到屏幕双击缩放的操作
        e.preventDefault();
        e.stopPropagation();
    }, []);

    // 避免 iframe 影响元素拖拽
    const [isElementDragging, setIsElementDragging] = useState(false);
    useStateSubscriber(ElementDraggingPublisher, setIsElementDragging);

    useEffect(() => {
        if (disabled || isElementDragging) {
            return;
        }

        const handleMessage = (event: MessageEvent) => {
            const { type } = event.data;

            if (type === 'contextMenu') {
                onContextMenu();
            } else if (type === 'wheel') {
                const wheelEvent = {
                    deltaY: event.data.eventData.deltaY,
                    clientX: event.data.eventData.clientX,
                    clientY: event.data.eventData.clientY,
                    ctrlKey: event.data.eventData.ctrlKey,
                    shiftKey: event.data.eventData.shiftKey,
                    altKey: event.data.eventData.altKey,
                } as React.WheelEvent<HTMLDivElement>;
                onWheel?.(wheelEvent);
            } else if (type === 'keydown' || type === 'keyup') {
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
            } else if (type === 'mousedown') {
                // 重新组装鼠标事件对象，模拟React.MouseEvent
                const mouseEvent = {
                    clientX: event.data.clientX,
                    clientY: event.data.clientY,
                    button: event.data.button,
                    buttons: event.data.buttons,
                    ctrlKey: event.data.ctrlKey,
                    shiftKey: event.data.shiftKey,
                    altKey: event.data.altKey,
                    metaKey: event.data.metaKey,
                    preventDefault: () => {},
                    stopPropagation: () => {},
                } as React.MouseEvent<HTMLDivElement>;
                onMouseDown?.(mouseEvent);
            } else if (type === 'mousemove') {
                // 重新组装鼠标移动事件对象
                const mouseEvent = {
                    clientX: event.data.clientX,
                    clientY: event.data.clientY,
                    button: event.data.button,
                    buttons: event.data.buttons,
                    ctrlKey: event.data.ctrlKey,
                    shiftKey: event.data.shiftKey,
                    altKey: event.data.altKey,
                    metaKey: event.data.metaKey,
                    preventDefault: () => {},
                    stopPropagation: () => {},
                } as React.MouseEvent<HTMLDivElement>;
                onMouseMove?.(mouseEvent);
            } else if (type === 'mouseup') {
                // 重新组装鼠标释放事件对象
                const mouseEvent = {
                    clientX: event.data.clientX,
                    clientY: event.data.clientY,
                    button: event.data.button,
                    buttons: event.data.buttons,
                    ctrlKey: event.data.ctrlKey,
                    shiftKey: event.data.shiftKey,
                    altKey: event.data.altKey,
                    metaKey: event.data.metaKey,
                    preventDefault: () => {},
                    stopPropagation: () => {},
                } as React.MouseEvent<HTMLDivElement>;
                onMouseUp?.(mouseEvent);
            }
        };

        window.addEventListener('message', handleMessage);

        return () => {
            window.removeEventListener('message', handleMessage);
        };
    }, [disabled, isElementDragging, onContextMenu, onMouseDown, onMouseMove, onMouseUp, onWheel]);

    const handleContainerContextMenu = useCallback((event: React.MouseEvent<HTMLDivElement>) => {
        event.preventDefault();
        event.stopPropagation();
    }, []);

    const enableDrag = !!(onMouseDown && onMouseMove && onMouseUp);

    return (
        <>
            <div
                style={{
                    zIndex: zIndex,
                    position: 'fixed',
                    top: 0,
                    left: 0,
                    width: '100%',
                    height: '100%',
                    ...style,
                }}
                className="ocr-result-container"
                ref={containerElementRef}
                onContextMenu={handleContainerContextMenu}
                onWheel={onWheel}
                onMouseDown={onMouseDown}
            >
                <div
                    ref={textContainerElementRef}
                    style={{
                        transformOrigin: 'top left',
                        position: 'absolute',
                        pointerEvents: 'none',
                    }}
                    onDoubleClick={onDoubleClick}
                    className="ocr-result-text-container"
                ></div>
                <div
                    style={{
                        transformOrigin: 'top left',
                        position: 'absolute',
                        opacity: textContainerContent ? 1 : 0,
                        userSelect: 'none',
                        pointerEvents:
                            isElementDragging || disabled || !textContainerContent
                                ? 'none'
                                : 'auto',
                    }}
                    ref={textIframeContainerElementWrapRef}
                >
                    <iframe
                        ref={textIframeContainerElementRef}
                        style={{
                            width: '100%',
                            height: '100%',
                            backgroundColor: 'transparent',
                        }}
                        className="ocr-result-text-iframe"
                        srcDoc={
                            textContainerContent
                                ? `<head><meta name="color-scheme" content="light dark"></meta></head>
                                <body>${textContainerContent}</body>
                        <style>
                            html {
                                height: 100%;
                                width: 100%;
                                background-color: transparent;
                            }

                            .ocr-result-text-background-element {
                                opacity: 0;
                            }

                            .ocr-result-text-element {
                                opacity: 1;
                            }

                            body {
                                height: 100%;
                                width: 100%;
                                padding: 0;
                                margin: 0;
                                border: none;
                                overflow: hidden;
                                ${enableDrag ? 'cursor: grab;' : ''}
                                background-color: transparent;
                            }
                            body:active {
                                ${enableDrag ? 'cursor: grabbing;' : ''}
                            }

                            * {
                                -webkit-user-select: text !important;
                                -moz-user-select: text !important;
                                -ms-user-select: text !important;
                                user-select: text !important;
                            }
                        </style>
                        <script>
                            document.oncopy = (e) => {
                                if (${enableCopy ? 'true' : 'false'}) {
                                    return;
                                }

                                e.preventDefault();
                            };

                            document.oncontextmenu = (e) => {
                                e.preventDefault();
                                e.stopPropagation();
                                window.parent.postMessage({
                                    type: 'contextMenu',
                                    eventData: {
                                        type: 'contextmenu',
                                        clientX: e.clientX,
                                        clientY: e.clientY,
                                    }
                                }, '*');
                            };

                            document.addEventListener('mousedown', (e) => {
                                window.parent.postMessage({
                                    type: 'mousedown',
                                    clientX: e.clientX,
                                    clientY: e.clientY,
                                    button: e.button,
                                    buttons: e.buttons,
                                    ctrlKey: e.ctrlKey,
                                    shiftKey: e.shiftKey,
                                    altKey: e.altKey,
                                    metaKey: e.metaKey,
                                }, '*');
                            });

                            document.addEventListener('wheel', (e) => {
                                e.preventDefault();
                                window.parent.postMessage({
                                    type: 'wheel',
                                    eventData: {
                                        deltaY: e.deltaY,
                                        clientX: e.clientX,
                                        clientY: e.clientY,
                                        ctrlKey: e.ctrlKey,
                                        shiftKey: e.shiftKey,
                                        altKey: e.altKey,
                                    },
                                }, '*');
                            });

                            document.addEventListener('mousemove', (e) => {
                                window.parent.postMessage({
                                    type: 'mousemove',
                                    clientX: e.clientX,
                                    clientY: e.clientY,
                                    button: e.button,
                                    buttons: e.buttons,
                                    ctrlKey: e.ctrlKey,
                                    shiftKey: e.shiftKey,
                                    altKey: e.altKey,
                                    metaKey: e.metaKey,
                                }, '*');
                            });

                            document.onmouseup = (e) => {
                                window.parent.postMessage({
                                    type: 'mouseup',
                                    clientX: e.clientX,
                                    clientY: e.clientY,
                                    button: e.button,
                                    buttons: e.buttons,
                                    ctrlKey: e.ctrlKey,
                                    shiftKey: e.shiftKey,
                                    altKey: e.altKey,
                                    metaKey: e.metaKey,
                                }, '*');
                            };

                            // 转发键盘事件到父窗口
                            document.onkeydown = (e) => {
                                window.parent.postMessage({
                                    type: 'keydown',
                                    key: e.key,
                                    code: e.code,
                                    keyCode: e.keyCode,
                                    ctrlKey: e.ctrlKey,
                                    shiftKey: e.shiftKey,
                                    altKey: e.altKey,
                                    metaKey: e.metaKey,
                                    repeat: e.repeat,
                                }, '*');
                            };

                            document.onkeyup = (e) => {
                                window.parent.postMessage({
                                    type: 'keyup',
                                    key: e.key,
                                    code: e.code,
                                    keyCode: e.keyCode,
                                    ctrlKey: e.ctrlKey,
                                    shiftKey: e.shiftKey,
                                    altKey: e.altKey,
                                    metaKey: e.metaKey,
                                }, '*');
                            };
                        </script>
                    `
                                : undefined
                        }
                    />
                </div>

                <style jsx>{`
                    .ocr-result-text-iframe {
                        width: 100%;
                        height: 100%;
                        padding: 0;
                        margin: 0;
                        border: none;
                    }

                    :global(.ocr-result-text-background-element) {
                        backdrop-filter: blur(2.4px);
                    }

                    :global(.ocr-result-text-element) {
                        opacity: 0;
                    }
                `}</style>
            </div>
        </>
    );
};
