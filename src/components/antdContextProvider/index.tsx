import { AntdContext } from '@/contexts/antdContext';
import { message, Modal } from 'antd';
import { HookAPI } from 'antd/es/modal/useModal';
import React, { useCallback, useMemo, useRef } from 'react';

export const AntdContextProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const [messageApi, messageContextHolder] = message.useMessage(
        useMemo(
            () => ({
                prefixCls: 'app-global-message',
            }),
            [],
        ),
    );
    const [modalApi, modalContextHolder] = Modal.useModal();

    const isConfirmingRef = useRef(false);
    const confirmWithStatus = useCallback(
        async (...params: Parameters<HookAPI['confirm']>) => {
            isConfirmingRef.current = true;
            const res = await modalApi.confirm(...params);
            isConfirmingRef.current = false;
            return res;
        },
        [modalApi],
    );

    const contextValues = useMemo(
        () => ({
            message: messageApi,
            modal: { ...modalApi, confirmWithStatus },
            isConfirmingRef,
        }),
        [messageApi, modalApi, confirmWithStatus, isConfirmingRef],
    );
    return (
        <AntdContext.Provider value={contextValues}>
            {messageContextHolder}
            {modalContextHolder}
            {children}
        </AntdContext.Provider>
    );
};
