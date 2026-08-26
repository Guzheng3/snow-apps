import type { GlobalToken } from "antd";
import { OcrResultType } from ".";

const domParser = new DOMParser();
export const getOcrResultIframeSrcDoc = (
	textContent: string,
	ocrResultType: OcrResultType,
	enableDrag: boolean | undefined,
	enableCopy: boolean | undefined,
	token: GlobalToken | undefined,
) => {
	let extraStyle = "";
	if (ocrResultType === OcrResultType.VisionModelHtml) {
		// 如果 textContent 包含 markdown 代码块标记，提取首个 < 和最后一个 > 之间的内容
		const firstLtIndex = textContent.indexOf("<");
		const lastGtIndex = textContent.lastIndexOf(">");

		textContent = textContent.substring(
			firstLtIndex === -1 ? 0 : firstLtIndex,
			lastGtIndex === -1 ? textContent.length : lastGtIndex + 1,
		);

		const contentHtmlDom = domParser.parseFromString(textContent, "text/html");
		textContent = contentHtmlDom.body.innerHTML;
		extraStyle = `
            table {
                border-collapse: collapse;
                border-spacing: 0;
                width: 100%;
                border: 1px solid ${token?.colorBorder ?? "#d9d9d9"};
            }

            th, td {
                text-align: left;
                padding: 8px 12px;
                border: 1px solid ${token?.colorBorder ?? "#d9d9d9"};
            }

            th {
                background-color: ${token?.colorBgContainer ? `color-mix(in srgb, ${token.colorBgContainer} 50%, ${token.colorBorderSecondary ?? "#f0f0f0"})` : "#fafafa"};
                font-weight: 600;
            }

            tr:hover {
                background-color: ${token?.colorBgTextHover ?? "rgba(0, 0, 0, 0.02)"};
            }
        `;
	}

	return `<head><meta name="color-scheme" content="light dark"></meta></head>
                                <body>${textContent}</body>
                        <style>
                            html {
                                height: 100%;
                                width: 100%;
                                background-color: ${ocrResultType === OcrResultType.VisionModelHtml ? (token?.colorBgContainer ?? "transparent") : "transparent"};
                                ${ocrResultType === OcrResultType.VisionModelHtml ? `color: ${token?.colorText ?? "inherit"}` : ""}
                            }

                            ${extraStyle}

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
                                overflow: ${ocrResultType === OcrResultType.VisionModelHtml ? "auto" : "hidden"};
                                ${enableDrag ? "cursor: grab;" : ""}
                                background-color: transparent;
                            }
                            body:active {
                                ${enableDrag ? "cursor: grabbing;" : ""}
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
                                if (${enableCopy ? "true" : "false"}) {
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
                    `;
};

/**
 * 将译文按来源文本的长度占比切分，使两者段数一致且占比尽量接近
 */

