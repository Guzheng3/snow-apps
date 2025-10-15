import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { HdrColorAlgorithm } from '@/types/appSettings';
import { getPlatform } from './platform';

export const getCorrectHdrColorAlgorithm = (appSettings: AppSettingsData) => {
    return getPlatform() === 'windows' &&
        appSettings[AppSettingsGroup.SystemScreenshot].correctHdrColor
        ? appSettings[AppSettingsGroup.SystemScreenshot].correctHdrColorAlgorithm
        : HdrColorAlgorithm.None;
};
