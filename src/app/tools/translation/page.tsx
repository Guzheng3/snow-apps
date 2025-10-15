'use client';

import { AppSettingsGroup } from '@/types/appSettings';
import { HotkeysMenu } from '@/components/hotkeysMenu';
import { finishScreenshot } from '@/functions/screenshot';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { copyText, copyTextAndHide } from '@/utils/clipboard';
import { theme } from 'antd';
import { useSearchParams } from 'next/navigation';
import { useCallback, useEffect, useRef, useState, Suspense } from 'react';
import { useHotkeys } from 'react-hotkeys-hook';
import { FormattedMessage } from 'react-intl';
import { ContentWrap } from '@/components/contentWrap';
import { Translator, TranslatorActionType } from '@/components/translator';
import { formatKey } from '@/utils/format';
import { decodeParamsValue } from '@/utils/base64';
import { CommonKeyEventKey, CommonKeyEventValue } from '@/types/core/commonKeyEvent';

const TranslationCore = () => {
    const { token } = theme.useToken();

    const translatorActionRef = useRef<TranslatorActionType>(undefined);

    const searchParams = useSearchParams();
    const searchParamsSign = searchParams.get('t');
    const searchParamsSelectText = searchParams.get('selectText');
    const prevSearchParamsSign = useRef<string | null>(null);
    const ignoreDebounce = useRef<boolean>(false);
    const updateSourceContentBySelectedText = useCallback(async () => {
        if (prevSearchParamsSign.current === searchParamsSign) {
            return;
        }

        prevSearchParamsSign.current = searchParamsSign;

        if (searchParamsSelectText) {
            await finishScreenshot();

            translatorActionRef.current?.setSourceContent(
                decodeParamsValue(searchParamsSelectText).substring(0, 5000),
                true,
            );
            ignoreDebounce.current = true;
        }

        setTimeout(() => {
            translatorActionRef.current?.getSourceContentRef()?.focus();
            prevSearchParamsSign.current = searchParamsSign;
        }, 64);
    }, [searchParamsSign, searchParamsSelectText]);

    useEffect(() => {
        updateSourceContentBySelectedText();
    }, [updateSourceContentBySelectedText]);

    const [hotKeys, setHotKeys] = useState<Record<CommonKeyEventKey, CommonKeyEventValue>>();
    useAppSettingsLoad(
        useCallback((appSettings) => {
            setHotKeys(appSettings[AppSettingsGroup.CommonKeyEvent]);
        }, []),
        true,
    );

    const onCopy = useCallback(() => {
        if (!translatorActionRef.current) {
            return;
        }

        copyText(translatorActionRef.current.getTranslatedContentRef());
    }, [translatorActionRef]);
    const onCopyAndHide = useCallback(() => {
        if (!translatorActionRef.current) {
            return;
        }

        copyTextAndHide(translatorActionRef.current.getTranslatedContentRef());
    }, [translatorActionRef]);

    useHotkeys(hotKeys?.[CommonKeyEventKey.CopyAndHide]?.hotKey ?? '', onCopyAndHide, {
        keyup: false,
        keydown: true,
        preventDefault: true,
        enableOnFormTags: ['INPUT', 'TEXTAREA', 'SELECT'],
    });
    useHotkeys(hotKeys?.[CommonKeyEventKey.Copy]?.hotKey ?? '', onCopy, {
        keyup: false,
        keydown: true,
        preventDefault: true,
        enableOnFormTags: ['INPUT', 'TEXTAREA', 'SELECT'],
    });

    return (
        <>
            <ContentWrap className="settings-wrap">
                <Translator actionRef={translatorActionRef} />

                <HotkeysMenu
                    className="translation-hotkeys-menu"
                    menu={{
                        items: [
                            {
                                label: (
                                    <FormattedMessage
                                        id="settings.hotKeySettings.keyEventTooltip"
                                        values={{
                                            message: (
                                                <FormattedMessage id="tools.translation.copy" />
                                            ),
                                            key: formatKey(
                                                hotKeys?.[CommonKeyEventKey.Copy]?.hotKey,
                                            ),
                                        }}
                                    />
                                ),
                                key: 'copy',
                                onClick: onCopy,
                            },
                            {
                                label: (
                                    <FormattedMessage
                                        id="settings.hotKeySettings.keyEventTooltip"
                                        values={{
                                            message: (
                                                <FormattedMessage id="tools.translation.copyAndHide" />
                                            ),
                                            key: formatKey(
                                                hotKeys?.[CommonKeyEventKey.CopyAndHide]?.hotKey,
                                            ),
                                        }}
                                    />
                                ),
                                key: 'copyAndHide',
                                onClick: onCopyAndHide,
                            },
                        ],
                    }}
                />
            </ContentWrap>

            <style jsx>{`
                :global(.translation-hotkeys-menu) {
                    position: fixed;
                    bottom: 0;
                    right: 0;
                    padding: ${token.padding}px;
                }
            `}</style>
        </>
    );
};

export default function TranslationPage() {
    return (
        <Suspense>
            <TranslationCore />
        </Suspense>
    );
}
