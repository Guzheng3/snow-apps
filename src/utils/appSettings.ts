import { AppSettingsData, AppSettingsGroup } from '@/app/contextWrap';
import { HdrColorAlgorithm } from '@/commands/screenshot';
import { getPlatform } from '.';

export const getCorrectHdrColorAlgorithm = (appSettings: AppSettingsData) => {
    return getPlatform() === 'windows' &&
        appSettings[AppSettingsGroup.SystemScreenshot].correctHdrColor
        ? appSettings[AppSettingsGroup.SystemScreenshot].correctHdrColorAlgorithm
        : HdrColorAlgorithm.None;
};
