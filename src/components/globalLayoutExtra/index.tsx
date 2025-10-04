import { message, Modal } from 'antd';
import { MessageInstance } from 'antd/es/message/interface';
import { HookAPI } from 'antd/es/modal/useModal';
import React, { useCallback, useContext, useEffect, useMemo, useRef } from 'react';

export type AntdContextType = {
    message: MessageInstance;
    modal: HookAPI & {
        confirmWithStatus: (...params: Parameters<HookAPI['confirm']>) => Promise<boolean>;
    };
    isConfirmingRef: React.RefObject<boolean>;
};

export const AntdContext = React.createContext<AntdContextType>({
    message: {} as MessageInstance,
    modal: {} as HookAPI & {
        confirmWithStatus: (...params: Parameters<HookAPI['confirm']>) => Promise<boolean>;
    },
    isConfirmingRef: {} as React.RefObject<boolean>,
});

export const AntdContextWrap: React.FC<{ children: React.ReactNode }> = ({ children }) => {
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

export const FetchErrorHandler: React.FC<{ children: React.ReactNode }> = ({ children }) => {
    const { message } = useContext(AntdContext);
    useEffect(() => {
        window.__APP_HANDLE_HTTP_ERROR__ = (error) => {
            message.error(`${error.response?.status}: ${error.response?.statusText}`);
        };
        window.__APP_HANDLE_SERVICE_ERROR__ = (error) => {
            message.error(`${error.code}: ${error.message}`);
        };
        window.__APP_HANDLE_REQUEST_ERROR__ = (error) => {
            message.error(`${error.code}: ${error.message}`);
        };
    }, [message]);
    return <>{children}</>;
};

export enum HotkeysScope {
    All = '*',
    DrawTool = 'draw_tool',
    None = 'none',
}
