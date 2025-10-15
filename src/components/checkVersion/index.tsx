import { useCallback, useEffect, useRef, useState } from 'react';
import { getVersion } from '@tauri-apps/api/app';
import { fetch } from '@tauri-apps/plugin-http';
import {
    isPermissionGranted,
    requestPermission,
    sendNotification,
} from '@tauri-apps/plugin-notification';
import { useIntl } from 'react-intl';
import { sendNewVersionNotification } from '@/commands/core';
import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { compare } from 'compare-versions';
import { getPlatform } from '@/utils/platform';
import { appError } from '@/utils/log';

const WEBSITE_URL = 'https://snowshot.top/';

export const getLatestVersion = async () => {
    const response = await fetch(`${WEBSITE_URL}latest-version.txt`);
    if (!response.ok) {
        appError('Failed to get latest version:', response.statusText);
        return;
    }

    return (await response.text()).trim();
};

export const CheckVersion: React.FC = () => {
    const intl = useIntl();
    const intervalRef = useRef<NodeJS.Timeout | null>(null);

    const clearIntervalRef = useCallback(() => {
        if (intervalRef.current) {
            clearInterval(intervalRef.current);
            intervalRef.current = null;
        }
    }, []);

    // 是否已经发送过通知
    const hasSendRef = useRef(false);
    const checkVersionCore = useCallback(async () => {
        try {
            const currentVersion = await getVersion();

            const latestVersion = await getLatestVersion();

            if (!latestVersion) {
                return;
            }

            if (compare(currentVersion, latestVersion, '>=')) {
                return;
            }

            if (hasSendRef.current) {
                return;
            }

            let permissionGranted = await isPermissionGranted();

            if (!permissionGranted) {
                const permission = await requestPermission();
                permissionGranted = permission === 'granted';
            }

            if (permissionGranted) {
                if (getPlatform() === 'macos') {
                    sendNotification({
                        title: intl.formatMessage(
                            { id: 'common.newVersion.title' },
                            {
                                latestVersion,
                            },
                        ),
                        body: intl.formatMessage(
                            { id: 'common.newVersion' },
                            {
                                latestVersion,
                                currentVersion,
                            },
                        ),
                    });
                } else {
                    sendNewVersionNotification(
                        intl.formatMessage(
                            { id: 'common.newVersion.title' },
                            {
                                latestVersion,
                            },
                        ),
                        intl.formatMessage(
                            { id: 'common.newVersion' },
                            {
                                latestVersion,
                                currentVersion,
                            },
                        ),
                    ).then(() => {
                        hasSendRef.current = true;
                        clearIntervalRef();
                    });
                }
            }
        } catch (error) {
            appError('Failed to check version:', error);
        }
    }, [clearIntervalRef, intl]);

    const checkVersionLoadingRef = useRef(false);
    const checkVersion = useCallback(async () => {
        if (checkVersionLoadingRef.current) {
            return;
        }

        checkVersionLoadingRef.current = true;
        await checkVersionCore();
        checkVersionLoadingRef.current = false;
    }, [checkVersionCore]);

    const [autoCheckVersion, setAutoCheckVersion] = useState<boolean | undefined>(undefined);
    useAppSettingsLoad(
        useCallback((appSettings: AppSettingsData) => {
            setAutoCheckVersion(appSettings[AppSettingsGroup.SystemCommon].autoCheckVersion);
        }, []),
        true,
    );

    const hasCheckedVersionRef = useRef(false);
    useEffect(() => {
        if (autoCheckVersion === undefined) {
            return;
        }

        if (autoCheckVersion) {
            if (!hasCheckedVersionRef.current) {
                checkVersion();
                hasCheckedVersionRef.current = true;
            }

            clearIntervalRef();

            intervalRef.current = setInterval(checkVersion, 1000 * 60 * 60);
        } else {
            clearIntervalRef();
        }

        return () => {
            if (intervalRef.current) {
                clearInterval(intervalRef.current);
                intervalRef.current = null;
            }
        };
    }, [autoCheckVersion, checkVersion, clearIntervalRef]);

    return <></>;
};
