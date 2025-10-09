'use client';

import { createContext, useCallback, useContext, useEffect, useMemo, useRef } from 'react';
import * as path from '@tauri-apps/api/path';
import { useStateRef } from '@/hooks/useStateRef';
import {
    pluginGetPluginsStatus,
    pluginInit,
    pluginRegisterPlugin,
    PluginStatusResult,
} from '@/commands/plugin';
import { getAppConfigBaseDirWithCache } from '@/commands/file';
import { isDeepEqualReact } from '@ant-design/pro-components';
import { throttle } from 'es-toolkit';
import { MenuLayoutContext } from '@/app/menuLayout';
import { getPlatform } from '@/utils';

export const PLUGIN_EVENT_PLUGIN_STATUS_CHANGE = 'plugin-status-change';

export const PLUGIN_ID_RAPID_OCR = 'rapid_ocr';
export const PLUGIN_ID_FFMPEG = 'ffmpeg';
export const PLUGIN_ID_AI_CHAT = 'ai_chat';

export type PluginItem = {
    id: string;
    file_list: string[];
};

export class PluginConfig {
    plugins: Map<string, PluginItem> = new Map();
    version: string = '';
    plugin_install_dir: string = '';
    plugin_download_dir: string = '';
    plugin_download_service_url: string = '';

    constructor(
        plugins: PluginItem[],
        version: string,
        plugin_install_dir: string,
        plugin_download_dir: string,
        plugin_download_service_url: string,
    ) {
        this.plugins = new Map(plugins.map((plugin) => [plugin.id, plugin]));
        this.version = version;
        this.plugin_install_dir = plugin_install_dir;
        this.plugin_download_dir = plugin_download_dir;
        this.plugin_download_service_url = plugin_download_service_url;
    }

    async getPluginDirPath(name: string) {
        return await path.join(this.plugin_install_dir, this.version, this.plugins.get(name)!.id);
    }
}

export enum PluginStatus {
    NotInstalled = 'NotInstalled',
    Installed = 'Installed',
    Downloading = 'Downloading',
    Unzipping = 'Unzipping',
    Uninstalling = 'Uninstalling',
}

export type PluginStatusRecord = Record<string, PluginStatusResult>;

export type PluginServiceContextType = {
    pluginConfig: PluginConfig | undefined;
    pluginConfigRef: React.RefObject<PluginConfig | undefined>;
    pluginStatus: PluginStatusRecord | undefined;
    pluginStatusRef: React.RefObject<PluginStatusRecord | undefined>;
    /** 通过 Ref 判断，避免组件重复渲染 */
    isReady: ((pluginId: string) => boolean) | undefined;
    /** 通过状态判断，触发组件重新渲染 */
    isReadyStatus: ((pluginId: string) => boolean) | undefined;
    refreshPluginStatus: () => void;
    refreshPluginStatusThrottle: () => void;
};

export const PluginServiceContext = createContext<PluginServiceContextType>({
    pluginConfig: new PluginConfig([], '', '', '', ''),
    pluginConfigRef: { current: undefined },
    pluginStatus: undefined,
    pluginStatusRef: { current: undefined },
    isReady: undefined,
    isReadyStatus: undefined,
    refreshPluginStatus: () => {},
    refreshPluginStatusThrottle: () => {},
});

export const usePluginService = () => {
    return useContext(PluginServiceContext);
};

export const PluginServiceProvider = ({ children }: { children: React.ReactNode }) => {
    const pluginList = useMemo<PluginItem[]>(() => {
        return [
            {
                id: PLUGIN_ID_RAPID_OCR,
                file_list: [
                    'ch_ppocr_mobile_v2.0_cls_infer.onnx',
                    'ch_PP-OCRv4_det_infer.onnx',
                    'ch_PP-OCRv4_rec_infer.onnx',
                    'ch_PP-OCRv5_rec_mobile_infer.onnx',
                ],
            },
            {
                id: PLUGIN_ID_FFMPEG,
                file_list: getPlatform() === 'windows' ? ['ffmpeg.exe'] : ['ffmpeg'],
            },
            {
                id: PLUGIN_ID_AI_CHAT,
                file_list: [],
            },
        ];
    }, []);

    const [pluginConfig, setPluginConfig, pluginConfigRef] = useStateRef<PluginConfig | undefined>(
        undefined,
    );
    const pluginStatusResultRef = useRef<PluginStatusResult[] | undefined>(undefined);
    const [pluginStatus, setPluginStatus, pluginStatusRef] = useStateRef<
        PluginStatusRecord | undefined
    >(undefined);
    const [pluginReadyStatus, setPluginReadyStatus, pluginReadyStatusRef] = useStateRef<
        Record<string, boolean> | undefined
    >(undefined);

    const { mainWindow } = useContext(MenuLayoutContext);

    const hasInitService = useRef(false);
    const initServiceReadyRef = useRef(false);
    const initPluginConfig = useCallback(async () => {
        const configDirPath = await getAppConfigBaseDirWithCache();

        const pluginConfig = new PluginConfig(
            pluginList,
            '20251005',
            await path.join(configDirPath, 'plugins'),
            await path.join(configDirPath, 'pluginsDownloads'),
            'https://snowshot.top/plugins/',
        );
        setPluginConfig(pluginConfig);

        if (!hasInitService.current) {
            hasInitService.current = true;

            if (mainWindow) {
                await pluginInit(
                    pluginConfig.version,
                    pluginConfig.plugin_install_dir,
                    pluginConfig.plugin_download_dir,
                    pluginConfig.plugin_download_service_url,
                );
                await Promise.all(
                    pluginList.map(async (plugin) => {
                        await pluginRegisterPlugin(plugin.id, plugin.file_list);
                    }),
                );
            }

            initServiceReadyRef.current = true;
        }
    }, [setPluginConfig, pluginList, mainWindow]);

    const refreshPluginStatus = useCallback(async () => {
        const pluginStatus = await pluginGetPluginsStatus();

        if (isDeepEqualReact(pluginStatus, pluginStatusResultRef.current)) {
            return;
        }

        pluginStatusResultRef.current = pluginStatus;

        setPluginStatus(
            pluginStatus.reduce((acc, plugin) => {
                acc[plugin.name] = plugin;
                return acc;
            }, {} as PluginStatusRecord),
        );

        const pluginReadyStatus = pluginStatus.reduce(
            (acc, plugin) => {
                acc[plugin.name] = plugin.status === PluginStatus.Installed;
                return acc;
            },
            {} as Record<string, boolean>,
        );

        if (isDeepEqualReact(pluginReadyStatus, pluginReadyStatusRef.current)) {
            return;
        }

        pluginReadyStatusRef.current = pluginReadyStatus;

        setPluginReadyStatus(pluginReadyStatus);
    }, [setPluginStatus, setPluginReadyStatus, pluginReadyStatusRef, pluginStatusResultRef]);

    const refreshPluginStatusThrottle = useMemo(
        () => throttle(refreshPluginStatus, 1000),
        [refreshPluginStatus],
    );

    const initPluginPendingRef = useRef(false);
    useEffect(() => {
        if (initPluginPendingRef.current) {
            return;
        }

        initPluginPendingRef.current = true;
        initPluginConfig().then(() => {
            refreshPluginStatus();
            initPluginPendingRef.current = false;
        });
    }, [initPluginConfig, refreshPluginStatus]);

    const isReadyCore = useCallback(
        (pluginId: string) => {
            return pluginReadyStatusRef.current?.[pluginId] ?? false;
        },
        [pluginReadyStatusRef],
    );

    const isReadyStatusCore = useCallback(
        (pluginId: string) => {
            return pluginReadyStatus?.[pluginId] ?? false;
        },
        [pluginReadyStatus],
    );

    const contextValues = useMemo(() => {
        return {
            pluginConfig,
            pluginConfigRef,
            pluginStatus,
            pluginStatusRef,
            refreshPluginStatus,
            refreshPluginStatusThrottle,
            isReady: pluginStatus ? isReadyCore : undefined,
            isReadyStatus: pluginStatus ? isReadyStatusCore : undefined,
        };
    }, [
        isReadyCore,
        pluginConfig,
        pluginConfigRef,
        pluginStatus,
        pluginStatusRef,
        refreshPluginStatus,
        refreshPluginStatusThrottle,
        isReadyStatusCore,
    ]);

    return (
        <PluginServiceContext.Provider value={contextValues}>
            {children}
        </PluginServiceContext.Provider>
    );
};
