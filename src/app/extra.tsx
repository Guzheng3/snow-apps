import { ButtonProps } from 'antd';
import { FormattedMessage } from 'react-intl';

export enum AppFunction {
    Screenshot = 'screenshot',
    ScreenshotFixed = 'screenshotFixed',
    ScreenshotOcr = 'screenshotOcr',
    /** 延迟 x 秒后截图 */
    ScreenshotDelay = 'screenshotDelay',
    /** 截图当前具有焦点的窗口 */
    ScreenshotFocusedWindow = 'screenshotFocusedWindow',
    /** 截图全屏 */
    ScreenshotFullScreen = 'screenshotFullScreen',
    /** 截图复制到剪贴板 */
    ScreenshotCopy = 'screenshotCopy',
    /** 截图翻译 */
    ScreenshotOcrTranslate = 'screenshotOcrTranslate',
    Chat = 'chat',
    ChatSelectText = 'chatSelectText',
    Translation = 'translation',
    TranslationSelectText = 'translationSelectText',
    FixedContent = 'fixedContent',
    VideoRecord = 'videoRecord',
    TopWindow = 'topWindow',
    FullScreenDraw = 'fullScreenDraw',
}

export enum AppFunctionGroup {
    Screenshot = 'screenshot',
    Translation = 'translation',
    Chat = 'chat',
    Other = 'other',
}

export type AppFunctionConfig = {
    shortcutKey: string;
    group: AppFunctionGroup;
};

export type AppFunctionComponentConfig = AppFunctionConfig & {
    configKey: AppFunction;
    title: React.ReactNode;
    icon?: React.ReactNode;
    group: AppFunctionGroup;
    onClick: () => Promise<void>;
    onKeyChange: (value: string, prevValue: string) => Promise<boolean>;
};

export const defaultAppFunctionConfigs: Record<AppFunction, AppFunctionConfig> = {
    [AppFunction.Screenshot]: {
        shortcutKey: 'F1',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotDelay]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotFixed]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotOcr]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotOcrTranslate]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotCopy]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotFullScreen]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ScreenshotFocusedWindow]: {
        shortcutKey: '',
        group: AppFunctionGroup.Screenshot,
    },
    [AppFunction.ChatSelectText]: {
        shortcutKey: '',
        group: AppFunctionGroup.Chat,
    },
    [AppFunction.Chat]: {
        shortcutKey: '',
        group: AppFunctionGroup.Chat,
    },
    [AppFunction.TranslationSelectText]: {
        shortcutKey: '',
        group: AppFunctionGroup.Translation,
    },
    [AppFunction.Translation]: {
        shortcutKey: '',
        group: AppFunctionGroup.Translation,
    },
    [AppFunction.FixedContent]: {
        shortcutKey: '',
        group: AppFunctionGroup.Other,
    },
    [AppFunction.VideoRecord]: {
        shortcutKey: '',
        group: AppFunctionGroup.Other,
    },
    [AppFunction.TopWindow]: {
        shortcutKey: '',
        group: AppFunctionGroup.Other,
    },

    [AppFunction.FullScreenDraw]: {
        shortcutKey: '',
        group: AppFunctionGroup.Other,
    },
};

export enum ShortcutKeyStatus {
    Registered = 'registered',
    Unregistered = 'unregistered',
    Error = 'error',
    None = 'none',
    PrintScreen = 'printScreen',
}

export const convertShortcutKeyStatusToButtonColor = (
    status: ShortcutKeyStatus | undefined,
): ButtonProps['color'] => {
    if (status === undefined) {
        return 'danger';
    }

    switch (status) {
        case ShortcutKeyStatus.PrintScreen:
        case ShortcutKeyStatus.Registered:
            return 'green';
        case ShortcutKeyStatus.Unregistered:
            return 'orange';
        case ShortcutKeyStatus.Error:
            return 'danger';
        default:
            return 'default';
    }
};

export const convertShortcutKeyStatusToTip = (
    status: ShortcutKeyStatus | undefined,
): React.ReactNode | undefined => {
    if (status === undefined || status === ShortcutKeyStatus.Registered) {
        return undefined;
    }

    switch (status) {
        case ShortcutKeyStatus.Unregistered:
            return <FormattedMessage id="home.shortcut.unregistered" />;
        case ShortcutKeyStatus.Error:
            return <FormattedMessage id="home.shortcut.error" />;
        case ShortcutKeyStatus.None:
            return <FormattedMessage id="home.shortcut.none" />;
        case ShortcutKeyStatus.PrintScreen:
            return <FormattedMessage id="settings.printScreen.tip" />;
        default:
            return undefined;
    }
};
