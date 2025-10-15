'use client';

import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { initUiElements } from '@/commands';
import { autoStartDisable, autoStartEnable, setEnableProxy, setRunLog } from '@/commands/core';
import { ocrInit } from '@/commands/ocr';
import { hotLoadPageInit } from '@/commands/hotLoadPage';
import { videoRecordInit } from '@/commands/videoRecord';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { usePluginServiceContext } from '@/contexts/pluginServiceContext';
import { PLUGIN_ID_FFMPEG, PLUGIN_ID_RAPID_OCR } from '@/constants/pluginService';
import { CaptureHistory } from '@/utils/captureHistory';
import { useCallback, useEffect, useRef, useState } from 'react';

export const InitService = () => {
    // 清除无效的截图历史
    const clearCaptureHistory = useCallback(async (appSettings: AppSettingsData) => {
        const captureHistory = new CaptureHistory();
        await captureHistory.init();
        await captureHistory.clearExpired(appSettings);
    }, []);

    const hasInitOcr = useRef(false);
    const hasClearedCaptureHistory = useRef(false);
    const hasInitAutoStart = useRef(false);
    const hasInitEnableProxy = useRef(false);
    const hasInitRunLog = useRef(false);
    const hasInitHotLoadPage = useRef(false);

    const [appSettings, setAppSettings] = useState<AppSettingsData | undefined>(undefined);
    const [prevAppSettings, setPrevAppSettings] = useState<AppSettingsData | undefined>(undefined);

    const { isReadyStatus, pluginConfigRef } = usePluginServiceContext();

    const initServices = useCallback(async () => {
        if (!appSettings || !isReadyStatus) {
            return;
        }

        if (
            (!hasInitOcr.current ||
                (prevAppSettings &&
                    (appSettings[AppSettingsGroup.SystemScreenshot].ocrModel !==
                        prevAppSettings[AppSettingsGroup.SystemScreenshot].ocrModel ||
                        appSettings[AppSettingsGroup.SystemScreenshot].ocrHotStart !==
                            prevAppSettings[AppSettingsGroup.SystemScreenshot].ocrHotStart ||
                        appSettings[AppSettingsGroup.SystemScreenshot].ocrModelWriteToMemory !==
                            prevAppSettings[AppSettingsGroup.SystemScreenshot]
                                .ocrModelWriteToMemory))) &&
            isReadyStatus(PLUGIN_ID_RAPID_OCR)
        ) {
            hasInitOcr.current = true;

            ocrInit(
                await pluginConfigRef.current!.getPluginDirPath(PLUGIN_ID_RAPID_OCR),
                appSettings[AppSettingsGroup.SystemScreenshot].ocrModel,
                appSettings[AppSettingsGroup.SystemScreenshot].ocrHotStart,
                appSettings[AppSettingsGroup.SystemScreenshot].ocrModelWriteToMemory,
            );
        }

        if (!hasClearedCaptureHistory.current) {
            hasClearedCaptureHistory.current = true;

            clearCaptureHistory(appSettings);
        }

        if (
            !hasInitEnableProxy.current ||
            (prevAppSettings &&
                appSettings[AppSettingsGroup.SystemNetwork].enableProxy !==
                    prevAppSettings[AppSettingsGroup.SystemNetwork].enableProxy)
        ) {
            hasInitEnableProxy.current = true;

            setEnableProxy(appSettings[AppSettingsGroup.SystemNetwork].enableProxy);
        }

        if (
            process.env.NODE_ENV !== 'development' &&
            (!hasInitAutoStart.current ||
                (prevAppSettings &&
                    appSettings[AppSettingsGroup.SystemCommon].autoStart !==
                        prevAppSettings[AppSettingsGroup.SystemCommon].autoStart))
        ) {
            hasInitAutoStart.current = true;

            if (appSettings[AppSettingsGroup.SystemCommon].autoStart) {
                autoStartEnable();
            } else {
                autoStartDisable();
            }
        }

        if (
            !hasInitRunLog.current ||
            (prevAppSettings &&
                appSettings[AppSettingsGroup.SystemCommon].runLog !==
                    prevAppSettings[AppSettingsGroup.SystemCommon].runLog)
        ) {
            hasInitRunLog.current = true;

            setRunLog(appSettings[AppSettingsGroup.SystemCommon].runLog);
        }

        if (
            !hasInitHotLoadPage.current ||
            (prevAppSettings &&
                appSettings[AppSettingsGroup.SystemCore].hotLoadPageCount !==
                    prevAppSettings[AppSettingsGroup.SystemCore].hotLoadPageCount)
        ) {
            hasInitHotLoadPage.current = true;

            hotLoadPageInit(appSettings[AppSettingsGroup.SystemCore].hotLoadPageCount);
        }
    }, [appSettings, clearCaptureHistory, pluginConfigRef, isReadyStatus, prevAppSettings]);

    useAppSettingsLoad(
        useCallback((appSettings, prevAppSettings) => {
            setAppSettings(appSettings);
            setPrevAppSettings(prevAppSettings);
        }, []),
        true,
    );

    const inited = useRef(false);

    useEffect(() => {
        if (inited.current) {
            return;
        }
        inited.current = true;

        initUiElements();
    }, []);

    useEffect(() => {
        initServices();
    }, [initServices]);

    const hasInitVideoRecord = useRef(false);
    useEffect(() => {
        if (hasInitVideoRecord.current) {
            return;
        }

        if (isReadyStatus?.(PLUGIN_ID_FFMPEG)) {
            hasInitVideoRecord.current = true;

            pluginConfigRef.current!.getPluginDirPath(PLUGIN_ID_FFMPEG).then((ffmpegPluginDir) => {
                videoRecordInit(ffmpegPluginDir);
            });
        }
    }, [isReadyStatus, pluginConfigRef]);

    return null;
};
