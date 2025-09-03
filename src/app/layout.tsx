'use client';

import './globals.css';
import { AntdRegistry } from '@ant-design/nextjs-registry';
import StyledJsxRegistry from './registry';
import { ContextWrap } from './contextWrap';
import { MenuLayout } from './menuLayout';
import Script from 'next/dist/client/script';
import { App as AntdApp } from 'antd';
import React, { useEffect } from 'react';
import { AntdContextWrap, FetchErrorHandler } from '@/components/globalLayoutExtra';
import { HotkeysProvider } from 'react-hotkeys-hook';

export default function RootLayout({
    children,
}: Readonly<{
    children: React.ReactNode;
}>) {
    useEffect(() => {
        const handleKeyDown = function (event: KeyboardEvent) {
            if (
                event.key === 'F5' ||
                (event.ctrlKey && event.key === 'r') ||
                (event.metaKey && event.key === 'r')
            ) {
                event.preventDefault();
            }
        };

        document.addEventListener('keydown', handleKeyDown);

        return () => {
            document.removeEventListener('keydown', handleKeyDown);
        };
    }, []);

    return (
        <html lang="zh-CN">
            <head>
                <Script id="load-env-variables" strategy="beforeInteractive">
                    {`window["EXCALIDRAW_ASSET_PATH"] = location.origin;`}
                </Script>
                <Script id="markdown-it-fix" strategy="beforeInteractive">
                    {`
                       if (typeof window !== 'undefined' && typeof window.isSpace === 'undefined') {
                         window.isSpace = function(code) {
                           return code === 0x20 || code === 0x09 || code === 0x0A || code === 0x0B || code === 0x0C || code === 0x0D;
                         };
                       }
                    `}
                </Script>
            </head>
            <body>
                <AntdApp>
                    <StyledJsxRegistry>
                        <AntdRegistry>
                            <ContextWrap>
                                <AntdContextWrap>
                                    <FetchErrorHandler>
                                        <HotkeysProvider>
                                            <MenuLayout>{children}</MenuLayout>
                                        </HotkeysProvider>
                                    </FetchErrorHandler>
                                </AntdContextWrap>
                            </ContextWrap>
                        </AntdRegistry>
                    </StyledJsxRegistry>
                </AntdApp>
            </body>
        </html>
    );
}
