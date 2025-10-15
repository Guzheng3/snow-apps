'use client';

import './globals.css';
import { AntdRegistry } from '@ant-design/nextjs-registry';
import StyledJsxRegistry from './registry';
import { MenuLayout } from '@/components/menuLayout';
import Script from 'next/dist/client/script';
import { App as AntdApp } from 'antd';
import React, { useEffect } from 'react';
import { FetchErrorHandler } from '@/components/fetchErrorHandler';
import { HotkeysProvider } from 'react-hotkeys-hook';
import { PluginServiceContextProvider } from '@/components/pluginServiceContextProvider';
import '@ant-design/v5-patch-for-react-19';
import { MenuLayoutContextProvider } from '@/components/menuLayoutContextProvider';
import { EventListener } from '@/components/eventListener';
import { AppSettingsContextProvider } from '@/components/appSettingsContextProvider';
import { TextScaleFactorProvider } from '@/components/textScaleFactorContextProvider';
import { AntdContextProvider } from '@/components/antdContextProvider';

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
                (event.metaKey && event.key === 'r') ||
                event.key === 'Alt' // 屏蔽 Alt + A, Alt + A 可能阻塞浏览器??? 逆天 Bug
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
                            <MenuLayoutContextProvider>
                                <PluginServiceContextProvider>
                                    <AppSettingsContextProvider>
                                        <TextScaleFactorProvider>
                                            <AntdContextProvider>
                                                <FetchErrorHandler>
                                                    <HotkeysProvider>
                                                        <EventListener>
                                                            <MenuLayout>{children}</MenuLayout>
                                                        </EventListener>
                                                    </HotkeysProvider>
                                                </FetchErrorHandler>
                                            </AntdContextProvider>
                                        </TextScaleFactorProvider>
                                    </AppSettingsContextProvider>
                                </PluginServiceContextProvider>
                            </MenuLayoutContextProvider>
                        </AntdRegistry>
                    </StyledJsxRegistry>
                </AntdApp>
            </body>
        </html>
    );
}
