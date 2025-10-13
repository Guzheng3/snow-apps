import { getCurrentWindow } from '@tauri-apps/api/window';
import {
    createContext,
    Dispatch,
    RefObject,
    SetStateAction,
    useCallback,
    useContext,
    useEffect,
    useState,
} from 'react';
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

const TextScaleFactorContext = createContext<{
    textScaleFactor: number;
    textScaleFactorRef: RefObject<number>;
    devicePixelRatio: number;
}>({
    textScaleFactor: 1,
    textScaleFactorRef: { current: 1 },
    devicePixelRatio: 1,
});

let useTextScaleFactorDataCache_textScaleFactor = 1;
let useTextScaleFactorDataCache_devicePixelRatio = 1;
export const TextScaleFactorProvider: React.FC<{
    children: React.ReactNode;
}> = ({ children }) => {
    const [textScaleFactor, setTextScaleFactor, textScaleFactorRef] = useStateRef(
        useTextScaleFactorDataCache_textScaleFactor,
    );
    const [devicePixelRatio, setDevicePixelRatio] = useState(
        useTextScaleFactorDataCache_devicePixelRatio,
    );

    const initTextScaleFactor = async (devicePixelRatio: number) => {
        const scaleFactor = await getCurrentWindow().scaleFactor();
        useTextScaleFactorDataCache_textScaleFactor = devicePixelRatio / scaleFactor;
        useTextScaleFactorDataCache_devicePixelRatio = devicePixelRatio;
        setTextScaleFactor(useTextScaleFactorDataCache_textScaleFactor);
        setDevicePixelRatio(useTextScaleFactorDataCache_devicePixelRatio);
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

    return (
        <TextScaleFactorContext.Provider
            value={{ textScaleFactor, textScaleFactorRef, devicePixelRatio }}
        >
            {children}
        </TextScaleFactorContext.Provider>
    );
};

/**
 * 获取文本缩放比例
 */
export const useTextScaleFactor = (): [number, number, RefObject<number>] => {
    const { textScaleFactor, textScaleFactorRef, devicePixelRatio } =
        useContext(TextScaleFactorContext);

    return [textScaleFactor, devicePixelRatio, textScaleFactorRef];
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

    const [uiScale, setUiScale] = useState<number>();
    const [toolbarUiScale, setToolbarUiScale] = useState<number>();

    useAppSettingsLoad(
        useCallback((settings) => {
            setUiScale(settings[AppSettingsGroup.Screenshot].uiScale);
            setToolbarUiScale(settings[AppSettingsGroup.Screenshot].toolbarUiScale);
        }, []),
        true,
    );

    useEffect(() => {
        if (!uiScale || !toolbarUiScale) {
            return;
        }

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
