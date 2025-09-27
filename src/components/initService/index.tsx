'use client';

import { AppSettingsData, AppSettingsGroup } from '@/app/contextWrap';
import { initUiElements } from '@/commands';
import { ocrInit } from '@/commands/ocr';
import { videoRecordInit } from '@/commands/videoRecord';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { CaptureHistory } from '@/utils/captureHistory';
import { useCallback, useEffect, useRef } from 'react';

export const InitService = () => {
    // 清除无效的截图历史
    const clearCaptureHistory = useCallback(async (appSettings: AppSettingsData) => {
        const captureHistory = new CaptureHistory();
        await captureHistory.init();
        await captureHistory.clearExpired(appSettings);
    }, []);

    const hasClearedCaptureHistory = useRef(false);
    useAppSettingsLoad(
        useCallback(
            (appSettings) => {
                ocrInit(appSettings[AppSettingsGroup.SystemScreenshot].ocrModel);

                if (!hasClearedCaptureHistory.current) {
                    hasClearedCaptureHistory.current = true;

                    clearCaptureHistory(appSettings);
                }
            },
            [clearCaptureHistory],
        ),
        true,
    );

    const inited = useRef(false);

    useEffect(() => {
        if (inited.current) {
            return;
        }
        inited.current = true;

        initUiElements();
        videoRecordInit();
    }, []);

    return null;
};
