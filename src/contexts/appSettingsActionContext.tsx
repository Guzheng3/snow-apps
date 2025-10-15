import { defaultAppSettingsData } from '@/constants/appSettings';
import { createPublisher } from '@/hooks/useStatePublisher';
import { AppSettingsData } from '@/types/appSettings';
import { AppSettingsActionContextType } from '@/types/contexts';
import { createContext } from 'react';

export const AppSettingsActionContext = createContext<AppSettingsActionContextType>({
    updateAppSettings: () => {},
    reloadAppSettings: () => {},
});

export const AppSettingsPublisher = createPublisher<AppSettingsData>(defaultAppSettingsData);

export const AppSettingsLoadingPublisher = createPublisher<boolean>(true);
