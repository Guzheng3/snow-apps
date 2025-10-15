'use client';

import { useCallback, useContext, useEffect, useMemo, useRef } from 'react';
import { useStateRef } from '@/hooks/useStateRef';
import { pluginGetPluginsStatus, pluginInit, pluginRegisterPlugin } from '@/commands/plugin';
import { getAppConfigBaseDirWithCache } from '@/utils/environment';
import { isDeepEqualReact } from '@ant-design/pro-components';
import { throttle } from 'es-toolkit';
import { getPlatform } from '@/utils/platform';
import { MenuLayoutContext } from '@/contexts/menuLayoutContext';
import { PluginItem, PluginConfig, PluginStatusRecord } from '@/types/components/pluginService';
import {
    PLUGIN_ID_AI_CHAT,
    PLUGIN_ID_FFMPEG,
    PLUGIN_ID_RAPID_OCR,
} from '@/constants/pluginService';
import { PluginStatus, PluginStatusResult } from '@/types/commands/plugin';
import * as path from '@tauri-apps/api/path';
import { PluginServiceContext } from '@/contexts/pluginServiceContext';

export const PluginServiceContextProvider = ({ children }: { children: React.ReactNode }) => {
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
