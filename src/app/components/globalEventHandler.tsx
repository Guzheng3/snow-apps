'use client';

import { EventListenerContext } from '@/components/eventListener';
import { showWindow } from '@/utils/window';
import { useContext, useEffect, useRef } from 'react';
import { getSelectedText } from '@/commands/core';
import { encodeParamsValue } from '@/utils/base64';
import { useRouter } from 'next/navigation';
import React from 'react';

const GlobalEventHandlerCore: React.FC = () => {
    const router = useRouter();

    const { addListener, removeListener } = useContext(EventListenerContext);

    const initedRef = useRef(false);
    useEffect(() => {
        if (initedRef.current) {
            return;
        }

        initedRef.current = true;

        const listenerIdList: number[] = [];
        listenerIdList.push(
            addListener('execute-chat', () => {
                showWindow();
                router.push(`/tools/chat?t=${Date.now()}`);
            }),
            addListener('execute-chat-selected-text', async () => {
                const text = (await getSelectedText()).substring(0, 10000);
                await showWindow();
                router.push(`/tools/chat?selectText=${encodeParamsValue(text)}&t=${Date.now()}`);
            }),
            addListener('execute-translate', () => {
                showWindow();
                router.push(`/tools/translation?t=${Date.now()}`);
            }),
            addListener('execute-translate-selected-text', async () => {
                const text = (await getSelectedText()).substring(0, 10000);
                await showWindow();
                router.push(
                    `/tools/translation?selectText=${encodeParamsValue(text)}&t=${Date.now()}`,
                );
            }),
        );

        return () => {
            listenerIdList.forEach((id) => {
                removeListener(id);
            });
        };
    }, [addListener, removeListener, router]);

    return <></>;
};

export const GlobalEventHandler = React.memo(GlobalEventHandlerCore);
