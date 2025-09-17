import { getCurrentWindow } from '@tauri-apps/api/window';
import { Dispatch, RefObject, SetStateAction, useCallback, useEffect, useState } from 'react';
import { useStateRef } from './useStateRef';
import { AppSettingsGroup } from '@/app/contextWrap';
import { useAppSettingsLoad } from './useAppSettingsLoad';

function listenDevicePixelRatio(callback: (ratio: number) => void) {
    const media = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);

    function handleChange() {
        callback(window.devicePixelRatio);
    }

    media.addEventListener('change', handleChange);

    return function stopListen() {
        media.removeEventListener('change', handleChange);
    };
}

/**
 * 获取文本缩放比例
 */
export const useTextScaleFactor = () => {
    const [textScaleFactor, setTextScaleFactor] = useState(1);
    const [devicePixelRatio, setDevicePixelRatio] = useState(1);

    const initTextScaleFactor = async (devicePixelRatio: number) => {
        const scaleFactor = await getCurrentWindow().scaleFactor();
        setTextScaleFactor(devicePixelRatio / scaleFactor);
        setDevicePixelRatio(devicePixelRatio);
    };

    useEffect(() => {
        initTextScaleFactor(window.devicePixelRatio);
        const stopListen = listenDevicePixelRatio((ratio) => {
            initTextScaleFactor(ratio);
        });
        return () => {
            stopListen();
        };
    }, []);

    return [textScaleFactor, devicePixelRatio];
};

/**
 * 计算内容缩放比例
 * @param monitorScaleFactor 显示器缩放比例
 * @param textScaleFactor 文本缩放比例
 * @param devicePixelRatio 设备像素比
 * @returns 内容缩放比例
 */
export const calculateContentScale = (
    monitorScaleFactor: number,
    textScaleFactor: number,
    devicePixelRatio: number,
) => {
    if (monitorScaleFactor === 0) {
        return 1;
    }

    return (monitorScaleFactor * textScaleFactor) / devicePixelRatio;
};

/**
 * 内容缩放比例
 * @returns 缩放比例
 */
export const useContentScale = (
    monitorScaleFactor: number,
    isToolbar?: boolean,
): [number, Dispatch<SetStateAction<number>>, RefObject<number>] => {
    const [textScaleFactor, devicePixelRatio] = useTextScaleFactor();
    const [contentScale, setContentScale, contentScaleRef] = useStateRef(1);

    const [uiScale, setUiScale] = useState(1);
    const [toolbarUiScale, setToolbarUiScale] = useState(1);

    useAppSettingsLoad(
        useCallback((settings) => {
            setUiScale(settings[AppSettingsGroup.Screenshot].uiScale);
            setToolbarUiScale(settings[AppSettingsGroup.Screenshot].toolbarUiScale);
        }, []),
        true,
    );

    useEffect(() => {
        setContentScale(
            calculateContentScale(monitorScaleFactor, textScaleFactor, devicePixelRatio) *
                (uiScale / 100) *
                (isToolbar ? toolbarUiScale / 100 : 1),
        );
    }, [
        devicePixelRatio,
        isToolbar,
        monitorScaleFactor,
        setContentScale,
        textScaleFactor,
        toolbarUiScale,
        uiScale,
    ]);

    return [contentScale, setContentScale, contentScaleRef];
};
