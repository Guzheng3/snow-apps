import { getCurrentWindow } from '@tauri-apps/api/window';
import { Dispatch, RefObject, SetStateAction, useEffect, useState } from 'react';
import { useStateRef } from './useStateRef';

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
): [number, Dispatch<SetStateAction<number>>, RefObject<number>] => {
    const [textScaleFactor, devicePixelRatio] = useTextScaleFactor();
    const [contentScale, setContentScale, contentScaleRef] = useStateRef(1);

    useEffect(() => {
        setContentScale(
            calculateContentScale(monitorScaleFactor, textScaleFactor, devicePixelRatio),
        );
    }, [monitorScaleFactor, textScaleFactor, devicePixelRatio, setContentScale]);

    return [contentScale, setContentScale, contentScaleRef];
};
