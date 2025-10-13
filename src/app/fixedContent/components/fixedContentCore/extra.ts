import { GlobalToken } from 'antd';

export const getHtmlContent = (token: GlobalToken, bodyContent: string) => {
    return `
      <html>
                  <head>
                  <meta name="color-scheme" content="light dark"></meta>
                  <style>
                        body {
                            width: fit-content;
                            height: fit-content;
                            margin: 0;
                            padding: ${token.padding}px;
                            overflow: hidden;
                            box-sizing: border-box;
                            background-color: transparent;
                            color: ${token.colorText};
                        }
                    </style>
                    <script>
                         window.addEventListener('load', () => {
                            window.parent.postMessage({
                                type: 'bodySize',
                                width: document.body.offsetWidth,
                                height: document.body.offsetHeight,
                                clientWidth: document.body.clientWidth,
                                clientHeight: document.body.clientHeight,
                            }, '*');
                        });

                        window.addEventListener('resize', () => {
                            window.parent.postMessage({
                                type: 'resize',
                                width: document.body.offsetWidth,
                                height: document.body.offsetHeight,
                                clientWidth: document.body.clientWidth,
                                clientHeight: document.body.clientHeight,
                            }, '*');
                        });

                        document.addEventListener('contextmenu', (e) => {
                            e.preventDefault();
                            window.parent.postMessage({
                                type: 'contextMenu',
                                x: e.clientX,
                                y: e.clientY
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

                        // 转发键盘事件到父窗口
                        document.addEventListener('keydown', (e) => {
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
                        });

                        document.addEventListener('keyup', (e) => {
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
                        });

                        // 拦截 a 标签的跳转操作
                        document.addEventListener('click', (e) => {
                            const target = e.target;
                            
                            // 检查点击的是否是 a 标签或其子元素
                            const linkElement = target.closest ? target.closest('a') : null;
                            
                            if (linkElement && linkElement.href) {
                                e.preventDefault(); // 阻止默认跳转行为
                                
                                window.parent.postMessage({
                                    type: 'linkClick',
                                    href: linkElement.href,
                                    text: linkElement.textContent || linkElement.innerText || '',
                                    target: linkElement.target || '_self'
                                }, '*');
                            }
                        });
                    </script>
                    </head>
                    <body>
                        ${bodyContent}
                    </body>
                </html>
    `;
};
