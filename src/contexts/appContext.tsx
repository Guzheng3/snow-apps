import { AppSettingsTheme } from '@/types/appSettings';
import { createContext, RefObject } from 'react';
import { Window as AppWindow } from '@tauri-apps/api/window';

export type AppContextType = {
    appWindowRef: RefObject<AppWindow | undefined>;
    currentTheme: AppSettingsTheme;
    enableCompactLayout: boolean;
};

export const AppContext = createContext<AppContextType>({
    appWindowRef: { current: undefined },
    currentTheme: AppSettingsTheme.Light,
    enableCompactLayout: false,
});
