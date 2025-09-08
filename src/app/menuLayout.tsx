'use client';

import React, {
    createContext,
    useCallback,
    useContext,
    useEffect,
    useMemo,
    useRef,
    useState,
} from 'react';
import {
    AppstoreOutlined,
    CloseOutlined,
    InfoCircleOutlined,
    MinusOutlined,
    SettingOutlined,
    ToolOutlined,
} from '@ant-design/icons';
import { Button, Layout, Menu, Space, TabsProps, theme } from 'antd';
import { useRouter } from 'next/navigation';
const { Content, Sider } = Layout;
import { usePathname } from 'next/navigation';
import RSC from 'react-scrollbars-custom';
import {
    AppContext,
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsLanguage,
    AppSettingsTheme,
} from './contextWrap';
import { Header } from 'antd/es/layout/layout';
import { getCurrentWindow, Window as AppWindow } from '@tauri-apps/api/window';
import '@ant-design/v5-patch-for-react-19';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { zhHans } from '@/messages/zhHans';
import { useIntl } from 'react-intl';
import { TrayIconLoader, TrayIconStatePublisher } from './trayIcon';
import { EventListener } from '@/components/eventListener';
import { zhHant } from '@/messages/zhHant';
import { en } from '@/messages/en';
import { ItemType, MenuItemType } from 'antd/es/menu/interface';
import { PageNav, PageNavActionType } from './components/pageNav';
import { GlobalEventHandler } from './components/globalEventHandler';
import { withStatePublisher } from '@/hooks/useStatePublisher';
import { CheckVersion } from '@/components/checkVersion';
import { InitService } from '@/components/initService';
import * as tauriOs from '@tauri-apps/plugin-os';
import { getPlatformValue } from '@/utils';
import { GlobalShortcut } from '@/components/globalShortcut';

type MenuItem = ItemType<MenuItemType>;

export type RouteMapItem = {
    items: TabsProps['items'];
    hideTabs?: boolean;
};

export const MenuLayoutContext = createContext<{
    noLayout: boolean;
    mainWindow: boolean;
    pathname: string;
}>({
    noLayout: false,
    mainWindow: false,
    pathname: '/',
});

type RouteItem = {
    key: string;
    path: string | undefined;
    label: string;
    icon?: React.ReactNode;
    hideTabs?: boolean;
    children?: RouteItem[];
    tabs?: TabsProps['items'];
};

const MenuSiderCore: React.FC<{
    menuItems: MenuItem[];
    darkMode: boolean;
    pathname: string;
}> = ({ menuItems, darkMode, pathname }) => {
    const { token } = theme.useToken();
    const [collapsed, setCollapsed] = useState(false);
    useAppSettingsLoad(
        useCallback((settings: AppSettingsData) => {
            setCollapsed(settings[AppSettingsGroup.Cache].menuCollapsed);
        }, []),
    );
    const { updateAppSettings } = useContext(AppSettingsActionContext);

    useEffect(() => {
        if (process.env.NODE_ENV === 'development') {
            return;
        }

        window.oncontextmenu = (e) => {
            e.preventDefault();
            e.stopPropagation();
        };

        return () => {
            window.oncontextmenu = null;
        };
    }, []);

    const [currentPlatform, setCurrentPlatform] = useState<tauriOs.Platform | undefined>(undefined);
    useEffect(() => {
        setCurrentPlatform(tauriOs.platform());
    }, []);

    return (
        <Sider
            theme={darkMode ? 'dark' : 'light'}
            collapsible={currentPlatform !== 'macos'}
            collapsed={collapsed}
            onCollapse={(value) => {
                setCollapsed(value);
                updateAppSettings(
                    AppSettingsGroup.Cache,
                    { menuCollapsed: value },
                    true,
                    true,
                    false,
                );
            }}
        >
            {currentPlatform === 'macos' && (
                <div className="macos-title-bar-margin app-tauri-drag-region"></div>
            )}

            {currentPlatform !== 'macos' && (
                <div className="logo-wrap">
                    <div className="logo-text">
                        {collapsed ? (
                            <>
                                <div className="logo-text-highlight">S</div>
                                <div>now</div>
                            </>
                        ) : (
                            <>
                                <div className="logo-text-highlight">Snow</div>
                                <div>Shot</div>
                            </>
                        )}
                    </div>
                </div>
            )}
            <Menu
                defaultSelectedKeys={[menuItems[0]!.key?.toString() ?? '/']}
                selectedKeys={[pathname]}
                mode="inline"
                items={menuItems}
                defaultOpenKeys={menuItems
                    .map((item) => item?.key as string)
                    .filter((key) => !!key)}
            />
            <style jsx>{`
                .logo-wrap {
                    margin-top: 16px;
                    margin-bottom: 10px;
                    font-weight: 600;
                    font-size: 21px;
                    text-align: center;
                    font-style: italic;
                }

                .logo-wrap .logo-text {
                    color: var(--snow-shot-text-color);
                    display: inline-block;
                    padding: 0px 12px;
                }

                :global(body) {
                    --snow-shot-purple-color: ${darkMode ? token['purple-7'] : token['purple-5']};
                    --snow-shot-text-color: ${darkMode ? '#fff' : '#000'};
                }

                .logo-wrap .logo-text .logo-text-highlight {
                    color: var(--snow-shot-purple-color);
                }

                .logo-wrap .logo-text div {
                    display: inline;
                }

                .macos-title-bar-margin {
                    width: 100%;
                    height: 32px;
                }
            `}</style>
        </Sider>
    );
};

const MenuSider = React.memo(MenuSiderCore);

const MenuContentCore: React.FC<{
    pathname: string;
    routeTabsMap: Record<string, RouteMapItem>;
    children: React.ReactNode;
}> = ({ pathname, routeTabsMap, children }) => {
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
                    <div data-tauri-drag-region className="logo-text">
                        <div data-tauri-drag-region className="logo-text-highlight">
                            Snow
                        </div>
                        <div data-tauri-drag-region>Shot</div>
                    </div>
                )}
            </Header>
            <Content>
                <div className="content-wrap">
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div data-tauri-drag-region className="app-tauri-drag-region"></div>
                    <div className="center">
                        <PageNav tabItems={tabItems} actionRef={pageNavActionRef} />
                        <RSC
                            onScroll={(e) => {
                                if ('scrollTop' in e && typeof e.scrollTop === 'number') {
                                    pageNavActionRef.current?.updateActiveKey(e.scrollTop);
                                }
                            }}
                        >
                            <div ref={contentRef} className="content-container">
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

            <style jsx>{`
                .content-wrap {
                    display: grid;
                    grid-template-columns: ${token.padding}px auto ${token.padding}px;
                    grid-template-rows: ${token.padding}px auto ${token.padding}px;
                    height: 100%;
                }

                .center {
                    grid-column: 2;
                    grid-row: 2;
                    overflow-y: hidden;
                    overflow-x: hidden;
                    border-radius: ${token.borderRadiusLG}px;
                    background-color: ${token.colorBgContainer} !important;
                    padding: ${token.padding}px ${token.borderRadiusLG}px;
                    display: flex;
                    flex-direction: column;
                    transform: translateY(0px);
                }

                .center::-webkit-scrollbar {
                    display: none;
                }

                .content-container {
                    padding: 0 ${token.padding}px;
                    width: 100%;
                    height: 100%;
                    overflow-x: hidden;
                }

                .logo-text {
                    position: absolute;
                    line-height: initial;
                    display: flex;
                    height: 32px;
                    align-items: center;
                    justify-content: center;
                    color: var(--snow-shot-text-color);
                    font-style: italic;
                    font-weight: 600;
                    user-select: none;
                    /* 对齐系统里的 title 位置 */
                    position: absolute;
                    left: 0;
                    right: 0;
                }

                .logo-text-highlight {
                    color: var(--snow-shot-purple-color);
                }
            `}</style>
        </Layout>
    );
};

const MenuContent = React.memo(MenuContentCore);

const MenuLayoutCore: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    useEffect(() => {
        if (process.env.NODE_ENV !== 'development') {
            return;
        }

        const zhHansKeys = Object.keys(zhHans);
        const zhHantKeys = new Set(Object.keys(zhHant));
        const enKeys = new Set(Object.keys(en));

        const zhHantMissingKeys: Record<string, string> = {};
        zhHansKeys
            .filter((key) => !zhHantKeys.has(key))
            .forEach((key) => {
                zhHantMissingKeys[key] = zhHans[key as keyof typeof zhHans];
            });

        const enMissingKeys: Record<string, string> = {};
        zhHansKeys
            .filter((key) => !enKeys.has(key))
            .forEach((key) => {
                enMissingKeys[key] = zhHans[key as keyof typeof zhHans];
            });

        console.log('App zh-Hant missing messages: ', zhHantMissingKeys);
        console.log('App en missing messages: ', enMissingKeys);
    }, []);

    const intl = useIntl();
    const appSettings = useContext(AppSettingsActionContext);
    const { currentTheme } = useContext(AppContext);
    const { updateAppSettings } = appSettings;

    const pathname = usePathname() || '/';
    useAppSettingsLoad(
        useCallback(
            (settings) => {
                // 获取浏览器语言，判断是否需要切换语言
                const settingBrowserLanguage = settings[AppSettingsGroup.Common].browserLanguage;
                const browserLanguage = navigator.language;
                if (settingBrowserLanguage !== browserLanguage) {
                    // 切换语言
                    let language = AppSettingsLanguage.EN;
                    if (browserLanguage.startsWith('zh')) {
                        if (browserLanguage.startsWith('zh-TW')) {
                            language = AppSettingsLanguage.ZHHant;
                        } else {
                            language = AppSettingsLanguage.ZHHans;
                        }
                    }

                    updateAppSettings(
                        AppSettingsGroup.Common,
                        {
                            browserLanguage: browserLanguage,
                            language,
                        },
                        false,
                        true,
                        true,
                    );
                }
            },
            [updateAppSettings],
        ),
    );

    const { token } = theme.useToken();

    const router = useRouter();
    const routes = useMemo(() => {
        const routes: RouteItem[] = [
            {
                key: '/',
                path: '/',
                label: intl.formatMessage({ id: 'menu.functions' }),
                icon: <AppstoreOutlined />,
                tabs: [
                    {
                        key: 'screenshotFunction',
                        label: intl.formatMessage({ id: 'home.screenshotFunction' }),
                    },
                    {
                        key: 'chatFunction',
                        label: intl.formatMessage({ id: 'home.chatFunction' }),
                    },
                    {
                        key: 'translationFunction',
                        label: intl.formatMessage({ id: 'home.translationFunction' }),
                    },
                    {
                        key: 'otherFunction',
                        label: intl.formatMessage({ id: 'home.otherFunction' }),
                    },
                ],
            },
            {
                key: '/tools',
                path: undefined,
                label: intl.formatMessage({ id: 'menu.tools' }),
                icon: <ToolOutlined />,
                tabs: [],
                children: [
                    {
                        key: '/tools/chat',
                        path: '/tools/chat',
                        label: intl.formatMessage({ id: 'menu.tools.chat' }),
                        hideTabs: true,
                        tabs: [
                            {
                                key: 'chat',
                                label: intl.formatMessage({ id: 'menu.tools.chat' }),
                            },
                        ],
                    },
                    {
                        key: '/tools/translation',
                        path: '/tools/translation',
                        label: intl.formatMessage({ id: 'menu.tools.translation' }),
                        hideTabs: true,
                        tabs: [
                            {
                                key: 'translation',
                                label: intl.formatMessage({ id: 'menu.tools.translation' }),
                            },
                        ],
                    },
                ],
            },
            {
                key: '/settings',
                path: undefined,
                label: intl.formatMessage({ id: 'menu.settings' }),
                icon: <SettingOutlined />,
                tabs: [],
                children: [
                    {
                        key: '/settings/generalSettings',
                        path: '/settings/generalSettings',
                        label: intl.formatMessage({ id: 'menu.settings.generalSettings' }),
                        tabs: [
                            {
                                key: 'commonSettings',
                                label: intl.formatMessage({ id: 'settings.commonSettings' }),
                            },
                            {
                                key: 'screenshotSettings',
                                label: intl.formatMessage({ id: 'settings.screenshotSettings' }),
                            },
                            {
                                key: 'trayIconSettings',
                                label: intl.formatMessage({
                                    id: 'settings.commonSettings.trayIconSettings',
                                }),
                            },
                        ],
                    },
                    {
                        key: '/settings/functionSettings',
                        path: '/settings/functionSettings',
                        label: intl.formatMessage({ id: 'menu.settings.functionSettings' }),
                        tabs: [
                            {
                                key: 'screenshotSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.screenshotSettings',
                                }),
                            },

                            {
                                key: 'fixedContentSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.fixedContentSettings',
                                }),
                            },
                            {
                                key: 'translationSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.translationSettings',
                                }),
                            },
                            {
                                key: 'chatSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.chatSettings',
                                }),
                            },
                            {
                                key: 'fullScreenDrawSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.fullScreenDrawSettings',
                                }),
                            },
                            {
                                key: 'videoRecordSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.videoRecordSettings',
                                }),
                            },
                            {
                                key: 'trayIconSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.trayIconSettings',
                                }),
                            },
                            {
                                key: 'outputSettings',
                                label: intl.formatMessage({
                                    id: 'settings.functionSettings.outputSettings',
                                }),
                            },
                        ],
                    },
                    {
                        key: '/settings/hotKeySettings',
                        path: '/settings/hotKeySettings',
                        label: intl.formatMessage({ id: 'menu.settings.hotKeySettings' }),
                        tabs: [
                            {
                                key: 'translation',
                                label: intl.formatMessage({
                                    id: 'settings.hotKeySettings.translation',
                                }),
                            },
                            {
                                key: 'chat',
                                label: intl.formatMessage({ id: 'settings.hotKeySettings.chat' }),
                            },
                            {
                                key: 'fixedContent',
                                label: intl.formatMessage({
                                    id: 'settings.hotKeySettings.fixedContent',
                                }),
                            },
                            {
                                key: 'drawingHotKey',
                                label: intl.formatMessage({ id: 'settings.drawingHotKey' }),
                            },
                        ],
                    },
                    {
                        key: '/settings/systemSettings',
                        path: '/settings/systemSettings',
                        label: intl.formatMessage({ id: 'menu.settings.systemSettings' }),
                        tabs: [
                            {
                                key: 'commonSettings',
                                label: intl.formatMessage({ id: 'settings.commonSettings' }),
                            },
                            ...getPlatformValue(
                                [],
                                [
                                    {
                                        key: 'macosPermissionsSettings',
                                        label: intl.formatMessage({
                                            id: 'settings.systemSettings.macosPermissionsSettings',
                                        }),
                                    },
                                ],
                            ),
                            {
                                key: 'screenshotSettings',
                                label: intl.formatMessage({
                                    id: 'settings.systemSettings.screenshotSettings',
                                }),
                            },
                            {
                                key: 'networkSettings',
                                label: intl.formatMessage({
                                    id: 'settings.systemSettings.networkSettings',
                                }),
                            },
                            {
                                key: 'scrollScreenshotSettings',
                                label: intl.formatMessage({
                                    id: 'settings.systemSettings.scrollScreenshotSettings',
                                }),
                            },
                            {
                                key: 'chatSettings',
                                label: intl.formatMessage({ id: 'settings.chatSettings' }),
                            },
                            {
                                key: 'dataFile',
                                label: intl.formatMessage({
                                    id: 'settings.systemSettings.dataFile',
                                }),
                            },
                        ],
                    },
                ],
            },
            {
                key: '/about',
                path: '/about',
                tabs: [
                    {
                        key: 'snowShot',
                        label: intl.formatMessage({ id: 'home.snowShot' }),
                    },
                ],
                label: intl.formatMessage({ id: 'menu.about' }),
                icon: <InfoCircleOutlined />,
            },
        ];

        return routes;
    }, [intl]);
    const { menuItems, routeTabsMap } = useMemo(() => {
        const routeTabsMap: Record<string, RouteMapItem> = {};

        const convertToMenuItem = (route: RouteItem): MenuItem => {
            const menuItem: MenuItem = {
                key: route.key,
                label: route.label,
                icon: route.icon,
                onClick: () => {
                    if (!route.path) {
                        return;
                    }

                    router.push(route.path!);
                },
                children: undefined as unknown as MenuItem[],
            };

            if (route.children?.length) {
                menuItem.children = route.children.map((child) => convertToMenuItem(child));
            }

            if (route.path && route.tabs?.length !== undefined) {
                routeTabsMap[route.path] = {
                    items: route.tabs,
                    hideTabs: route.hideTabs,
                };
            }

            return menuItem;
        };

        const menuItems = Object.values(routes).map(convertToMenuItem);

        return { menuItems, routeTabsMap };
    }, [router, routes]);

    return (
        <>
            <TrayIconLoader />
            <GlobalEventHandler />
            <CheckVersion />
            <div className="menu-layout-wrap">
                <Layout>
                    <MenuSider
                        menuItems={menuItems}
                        darkMode={currentTheme === AppSettingsTheme.Dark}
                        pathname={pathname}
                    />
                    <MenuContent pathname={pathname} routeTabsMap={routeTabsMap}>
                        <GlobalShortcut>{children}</GlobalShortcut>
                    </MenuContent>
                </Layout>
                <style jsx>{`
                    .menu-layout-wrap {
                        box-shadow: 0 0 12px 0 rgba(0, 0, 0, 0.21);
                        overflow: hidden;
                        height: 100%;
                    }

                    .menu-layout-wrap :global(.ant-layout) {
                        height: 100% !important;
                    }

                    .menu-layout-wrap > :global(.ant-layout) {
                        flex-direction: row !important;
                    }

                    .menu-layout-wrap :global(.ant-layout-sider-trigger) {
                        position: absolute !important;
                    }

                    .menu-layout-wrap :global(.ant-layout-sider) {
                        box-shadow: ${token.boxShadowSecondary};
                    }

                    .menu-layout-wrap :global(.ant-layout-header) {
                        height: 32px !important;
                        background: ${token.colorBgContainer} !important;
                        display: flex;
                        align-items: center;
                        justify-content: flex-end;
                        padding: 0 ${token.padding}px;
                    }
                `}</style>
            </div>
        </>
    );
};

const ML = React.memo(withStatePublisher(MenuLayoutCore, TrayIconStatePublisher));

export const MenuLayout = ({ children }: { children: React.ReactNode }) => {
    const pathname = usePathname();
    const noLayout = useMemo(
        () =>
            pathname === '/draw' ||
            pathname === '/fixedContent' ||
            pathname === '/fullScreenDraw' ||
            pathname === '/fullScreenDraw/switchMouseThrough' ||
            pathname === '/videoRecord' ||
            pathname === '/videoRecord/toolbar',
        [pathname],
    );
    const mainWindow = !noLayout;
    return (
        <MenuLayoutContext.Provider value={{ noLayout, pathname, mainWindow }}>
            <EventListener>
                {noLayout ? (
                    children
                ) : (
                    <>
                        <InitService />
                        <ML>{children}</ML>
                    </>
                )}
            </EventListener>
        </MenuLayoutContext.Provider>
    );
};
