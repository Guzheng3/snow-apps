'use client';

import React, { useCallback, useContext, useEffect, useState } from 'react';
import { Layout, Menu } from 'antd';
import { createStyles } from 'antd-style';
const { Sider } = Layout;
import RSC from 'react-scrollbars-custom';
import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { AppSettingsActionContext } from '@/contexts/appSettingsActionContext';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { ItemType, MenuItemType } from 'antd/es/menu/interface';
import * as tauriOs from '@tauri-apps/plugin-os';
import classNames from 'classnames';

type MenuItem = ItemType<MenuItemType>;

const useStyles = createStyles(() => ({
    logoWrap: {
        marginTop: 16,
        marginBottom: 10,
        fontWeight: 600,
        fontSize: 21,
        textAlign: 'center',
        fontStyle: 'italic',
    },
    logoText: {
        color: 'var(--snow-shot-text-color)',
        display: 'inline-block',
        padding: '0px 12px',
    },
    logoTextHighlight: {
        color: 'var(--snow-shot-purple-color)',
    },
    textDiv: {
        display: 'inline',
    },
    macosTitleBarMargin: {
        width: '100%',
        height: 32,
    },
    menuSiderWrap: {
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        '& .ScrollbarsCustom-Wrapper': {
            inset: '0 0 0 0 !important',
        },
    },
    menuWrap: {
        overflow: 'auto',
    },
}));

const MenuSiderCore: React.FC<{
    menuItems: MenuItem[];
    darkMode: boolean;
    pathname: string;
}> = ({ menuItems, darkMode, pathname }) => {
    const { styles } = useStyles();
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
            collapsed={collapsed}
            collapsible
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
            <div className={styles.menuSiderWrap}>
                {currentPlatform === 'macos' && (
                    <div className={`${styles.macosTitleBarMargin} app-tauri-drag-region`}></div>
                )}

                {currentPlatform !== 'macos' && (
                    <div className={styles.logoWrap}>
                        <div className={styles.logoText}>
                            {collapsed ? (
                                <>
                                    <div
                                        className={classNames(
                                            styles.logoTextHighlight,
                                            styles.textDiv,
                                        )}
                                    >
                                        S
                                    </div>
                                    <div className={styles.textDiv}>now</div>
                                </>
                            ) : (
                                <>
                                    <div
                                        className={classNames(
                                            styles.logoTextHighlight,
                                            styles.textDiv,
                                        )}
                                    >
                                        Snow
                                    </div>
                                    <div className={styles.textDiv}>Shot</div>
                                </>
                            )}
                        </div>
                    </div>
                )}
                <RSC>
                    <Menu
                        defaultSelectedKeys={[menuItems[0]!.key?.toString() ?? '/']}
                        selectedKeys={[pathname]}
                        mode="inline"
                        theme={darkMode ? 'dark' : 'light'}
                        items={menuItems}
                        defaultOpenKeys={menuItems
                            .map((item) => item?.key as string)
                            .filter((key) => !!key)}
                    />
                </RSC>
            </div>
        </Sider>
    );
};

export const MenuSider = React.memo(MenuSiderCore);
