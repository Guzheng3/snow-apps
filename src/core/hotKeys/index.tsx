import { getPlatformValue } from '@/utils';

export enum KeyEventGroup {
    Translation = 'translation',
    Chat = 'chat',
    FixedContent = 'fixedContent',
}

export type KeyEventValue = {
    hotKey: string;
    unique?: boolean;
    group: KeyEventGroup;
};

export type KeyEventComponentValue = KeyEventValue & {
    messageId: string;
};

export enum KeyEventKey {
    CopyAndHide = 'copyAndHide',
    Copy = 'copy',
    ChatCopyAndHide = 'chatCopyAndHide',
    ChatCopy = 'chatCopy',
    ChatNewSession = 'chatNewSession',
    FixedContentEnableDraw = 'fixedContentEnableDraw',
    FixedContentSwitchThumbnail = 'fixedContentSwitchThumbnail',
    FixedContentAlwaysOnTop = 'fixedContentAlwaysOnTop',
    FixedContentCloseWindow = 'fixedContentCloseWindow',
    FixedContentCopyToClipboard = 'fixedContentCopyToClipboard',
    FixedContentCopyRawToClipboard = 'fixedContentCopyRawToClipboard',
    FixedContentSaveToFile = 'fixedContentSaveToFile',
    FixedContentSelectText = 'fixedContentSelectText',
    FixedContentSetOpacity = 'fixedContentSetOpacity',
}

export const defaultKeyEventSettings: Record<KeyEventKey, KeyEventValue> = {
    [KeyEventKey.CopyAndHide]: {
        hotKey: getPlatformValue('Ctrl+Q', 'Meta+Q'),
        group: KeyEventGroup.Translation,
    },
    [KeyEventKey.Copy]: {
        hotKey: getPlatformValue('Ctrl+C', 'Meta+C'),
        group: KeyEventGroup.Translation,
    },
    [KeyEventKey.ChatCopyAndHide]: {
        hotKey: getPlatformValue('Ctrl+Q', 'Meta+Q'),
        group: KeyEventGroup.Chat,
    },
    [KeyEventKey.ChatCopy]: {
        hotKey: getPlatformValue('Ctrl+C', 'Meta+C'),
        group: KeyEventGroup.Chat,
    },
    [KeyEventKey.ChatNewSession]: {
        hotKey: getPlatformValue('Ctrl+N', 'Meta+N'),
        group: KeyEventGroup.Chat,
    },
    [KeyEventKey.FixedContentEnableDraw]: {
        hotKey: getPlatformValue('Ctrl+E', 'Meta+E'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentSwitchThumbnail]: {
        hotKey: 'R',
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentAlwaysOnTop]: {
        hotKey: getPlatformValue('Ctrl+T', 'Meta+T'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentCloseWindow]: {
        hotKey: 'Escape',
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentCopyToClipboard]: {
        hotKey: getPlatformValue('Ctrl+C', 'Meta+C'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentCopyRawToClipboard]: {
        hotKey: getPlatformValue('Ctrl+Shift+C', 'Meta+Shift+C'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentSaveToFile]: {
        hotKey: getPlatformValue('Ctrl+S', 'Meta+S'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentSelectText]: {
        hotKey: getPlatformValue('Ctrl+D', 'Meta+D'),
        group: KeyEventGroup.FixedContent,
    },
    [KeyEventKey.FixedContentSetOpacity]: {
        hotKey: getPlatformValue('Ctrl', 'Meta'),
        group: KeyEventGroup.FixedContent,
        unique: false,
    },
};

const keyEventSettingsKeys = Object.keys(defaultKeyEventSettings);
export const defaultKeyEventComponentConfig: Record<KeyEventKey, KeyEventComponentValue> =
    keyEventSettingsKeys.reduce(
        (acc, key) => {
            let baseMessageId = '';
            if (defaultKeyEventSettings[key as KeyEventKey].group === KeyEventGroup.Translation) {
                baseMessageId = 'tools.translation';
            } else if (defaultKeyEventSettings[key as KeyEventKey].group === KeyEventGroup.Chat) {
                baseMessageId = 'tools.chat';
            } else if (
                defaultKeyEventSettings[key as KeyEventKey].group === KeyEventGroup.FixedContent
            ) {
                baseMessageId = 'settings.hotKeySettings.fixedContent';
            }

            acc[key as KeyEventKey] = {
                ...defaultKeyEventSettings[key as KeyEventKey],
                messageId: `${baseMessageId}.${key}`,
            };
            return acc;
        },
        {} as Record<KeyEventKey, KeyEventComponentValue>,
    );
