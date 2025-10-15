'use client';

import React, { useEffect, useMemo, useRef, useState } from 'react';
import { CloseOutlined, MinusOutlined } from '@ant-design/icons';
import { Button, Layout, Space, theme } from 'antd';
import { createStyles } from 'antd-style';
import RSC from 'react-scrollbars-custom';
import { Header } from 'antd/es/layout/layout';
import { getCurrentWindow, Window as AppWindow } from '@tauri-apps/api/window';
import { PageNav, PageNavActionType } from '@/app/components/pageNav';
import * as tauriOs from '@tauri-apps/plugin-os';
import { RouteMapItem } from '@/types/components/menuLayout';

const { Content } = Layout;

const useStyles = createStyles(({ token }) => ({
    contentWrap: {
        display: 'grid',
        gridTemplateColumns: `${token.padding}px auto ${token.padding}px`,
        gridTemplateRows: `${token.padding}px auto ${token.padding}px`,
        height: '100%',
    },
    center: {
        gridColumn: 2,
        gridRow: 2,
        overflowY: 'hidden',
        overflowX: 'hidden',
        borderRadius: `${token.borderRadiusLG}px`,
        backgroundColor: `${token.colorBgContainer} !important`,
        padding: `${token.padding}px ${token.borderRadiusLG}px`,
        display: 'flex',
        flexDirection: 'column',
        transform: 'translateY(0px)',
        '&::-webkit-scrollbar': {
            display: 'none',
        },
    },
    contentContainer: {
        padding: `0 ${token.padding}px`,
        width: '100%',
        height: '100%',
        overflowX: 'hidden',
    },
    logoText: {
        position: 'absolute',
        lineHeight: 'initial',
        display: 'flex',
        height: 32,
        alignItems: 'center',
        justifyContent: 'center',
        color: 'var(--snow-shot-text-color)',
        fontStyle: 'italic',
        fontWeight: 600,
        userSelect: 'none',
        left: 0,
        right: 0,
    },
    logoTextHighlight: {
        color: 'var(--snow-shot-purple-color)',
    },
}));

const MenuContentCore: React.FC<{
    pathname: string;
    routeTabsMap: Record<string, RouteMapItem>;
    children: React.ReactNode;
}> = ({ pathname, routeTabsMap, children }) => {
    const { styles } = useStyles();
    const { token } = theme.useToken();
    const appWindowRef = useRef<AppWindow | undefined>(undefined);
    useEffect(() => {
        appWindowRef.current = getCurrentWindow();
    }, []);

    const tabItems = useMemo(() => {
        return routeTabsMap[pathname] ?? routeTabsMap['/'] ?? [];
    }, [pathname, routeTabsMap]);

    const pageNavActionRef = useRef<PageNavActionType | null>(null);
    const contentRef = useRef<HTMLDivElement>(null);

    const [currentPlatform, setCurrentPlatform] = useState<tauriOs.Platform | undefined>(undefined);
    useEffect(() => {
        setCurrentPlatform(tauriOs.platform());
    }, []);

    return (
        <Layout>
            <Header data-tauri-drag-region className="app-tauri-drag-region">
                {currentPlatform !== 'macos' && (
                    <Space>
                        <Button
                            type="text"
                            size="small"
                            icon={<MinusOutlined />}
                            onClick={() => {
                                appWindowRef.current?.minimize();
                            }}
                        />
                        <Button
                            type="text"
                            size="small"
                            icon={<CloseOutlined />}
                            onClick={() => {
                                appWindowRef.current?.hide();
                                appWindowRef.current?.emit('on-hide-main-window');
                            }}
                        />
                    </Space>
                )}

                {currentPlatform === 'macos' && (
                    <div data-tauri-drag-region className={styles.logoText}>
                        <div data-tauri-drag-region className={styles.logoTextHighlight}>
                            Snow
                        </div>
                        <div data-tauri-drag-region>Shot</div>
                    </div>
                )}
            </Header>
            <Content>
                <div className={styles.contentWrap}>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div className={styles.center}>
                        <PageNav tabItems={tabItems} actionRef={pageNavActionRef} />
                        <RSC
                            onScroll={(e) => {
                                if ('scrollTop' in e && typeof e.scrollTop === 'number') {
                                    pageNavActionRef.current?.updateActiveKey(e.scrollTop);
                                }
                            }}
                        >
                            <div ref={contentRef} className={styles.contentContainer}>
                                {children}
                            </div>
                        </RSC>
                    </div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                </div>
            </Content>
        </Layout>
    );
};

export const MenuContent = React.memo(MenuContentCore);
