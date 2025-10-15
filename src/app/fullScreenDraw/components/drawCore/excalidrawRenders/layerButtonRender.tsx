import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import React, { useCallback, useMemo, useState } from 'react';
import { Button } from 'antd';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { AppSettingsGroup } from '@/types/appSettings';
import { useIntl } from 'react-intl';
import { HotkeysScope } from '@/types/core/appHotKeys';
import { useHotkeysApp } from '@/hooks/useHotkeysApp';
import { DrawToolbarKeyEventKey } from '@/types/components/drawToolbar';

const DeleteSelectedElementsButton: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['layerButtonRender']
> = (props) => {
    const intl = useIntl();
    const [hotkey, setHotkey] = useState<string>('');

    useAppSettingsLoad(
        useCallback((appSettings) => {
            setHotkey(
                appSettings[AppSettingsGroup.DrawToolbarKeyEvent][DrawToolbarKeyEventKey.RemoveTool]
                    .hotKey,
            );
        }, []),
        true,
    );

    useHotkeysApp(
        hotkey,
        () => {
            props.onClick?.();
        },
        useMemo(
            () => ({
                keydown: true,
                keyup: false,
                preventDefault: true,
                enabled: hotkey !== '',
                scopes: HotkeysScope.DrawTool,
            }),
            [hotkey],
        ),
    );

    const buttonTitle = useMemo(() => {
        return intl.formatMessage(
            {
                id: 'draw.keyEventTooltip',
            },
            {
                message: props.title,
                key: hotkey,
            },
        );
    }, [hotkey, intl, props.title]);

    return <CommonButton {...props} title={buttonTitle} />;
};

const CommonButton: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['layerButtonRender']
> = (props) => {
    const { onClick, title, children, hidden, visible, active } = props;

    if ((hidden !== undefined && hidden) || (visible !== undefined && !visible)) {
        return <></>;
    }

    return (
        <Button
            title={title}
            type={active !== undefined ? (active ? 'default' : 'dashed') : undefined}
            variant={active === true ? 'outlined' : undefined}
            color={active === true ? 'primary' : undefined}
            onClick={onClick}
            style={{ margin: '0.25rem 0' }}
            icon={<div className="radio-button-icon">{children}</div>}
        />
    );
};

export const layerButtonRender: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['layerButtonRender']
> = (props) => {
    if (props.name === 'deleteSelectedElements') {
        return <DeleteSelectedElementsButton {...props} key={props.key ?? props.title} />;
    }

    return <CommonButton {...props} key={props.key ?? props.title} />;
};
