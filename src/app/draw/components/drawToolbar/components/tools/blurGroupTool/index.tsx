import { Button, Flex, theme } from 'antd';
import { useIntl } from 'react-intl';
import { useState, useCallback, useContext, useMemo } from 'react';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/app/fullScreenDraw/components/drawCore/extra';
import { getButtonTypeByState } from '../../../extra';
import { FilterFreeDrawIcon, FilterIcon } from '@/components/icons';
import {
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
} from '@/app/contextWrap';
import { ToolbarPopover } from '@/app/draw/components/drawToolbar/components/toolbarPopover';
import React from 'react';
import { KeyEventKey } from '../../keyEventWrap/extra';
import { ToolButton } from '../../toolButton';

const BlurGroupToolCore: React.FC<{
    customToolbarToolHiddenMap: Partial<Record<DrawState, boolean>> | undefined;
    onToolClickAction: (tool: DrawState) => void;
    disable: boolean;
}> = ({ customToolbarToolHiddenMap, onToolClickAction, disable }) => {
    const intl = useIntl();
    const { token } = theme.useToken();

    const { updateAppSettings } = useContext(AppSettingsActionContext);

    const [lastBlurTool, setLastBlurTool] = useState<DrawState>(DrawState.Blur);
    useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (settings: AppSettingsData) => {
                setLastBlurTool(settings[AppSettingsGroup.Cache].lastFilterTool);
            },
            [setLastBlurTool],
        ),
    );
    const [drawState, setDrawState] = useState(DrawState.Idle);
    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (state: DrawState) => {
                setDrawState(state);
            },
            [setDrawState],
        ),
    );

    const updateLastBlurTool = useCallback(
        (value: DrawState) => {
            updateAppSettings(
                AppSettingsGroup.Cache,
                { lastFilterTool: value },
                true,
                true,
                false,
                true,
                false,
            );
        },
        [updateAppSettings],
    );

    const blurButton = useMemo(() => {
        return (
            <ToolButton
                hidden={customToolbarToolHiddenMap?.[DrawState.Blur]}
                componentKey={KeyEventKey.BlurTool}
                icon={<FilterIcon />}
                drawState={DrawState.Blur}
                disable={disable}
                key="blur"
                onClick={() => {
                    onToolClickAction(DrawState.Blur);
                    updateLastBlurTool(DrawState.Blur);
                }}
            />
        );
    }, [disable, customToolbarToolHiddenMap, onToolClickAction, updateLastBlurTool]);

    const blurFreeDrawButton = useMemo(() => {
        return (
            <Button
                icon={<FilterFreeDrawIcon style={{ fontSize: '1em' }} />}
                title={intl.formatMessage({ id: 'draw.blurFreeDrawTool' })}
                type={getButtonTypeByState(drawState === DrawState.BlurFreeDraw)}
                key="blurFreeDraw"
                onClick={() => {
                    onToolClickAction(DrawState.BlurFreeDraw);
                    updateLastBlurTool(DrawState.BlurFreeDraw);
                }}
                disabled={disable}
            />
        );
    }, [disable, drawState, intl, onToolClickAction, updateLastBlurTool]);

    let mainToolbarButton = customToolbarToolHiddenMap?.[DrawState.Blur]
        ? blurFreeDrawButton
        : blurButton;
    if (lastBlurTool === DrawState.Blur && !customToolbarToolHiddenMap?.[DrawState.Blur]) {
        mainToolbarButton = blurButton;
    } else if (
        lastBlurTool === DrawState.BlurFreeDraw &&
        !customToolbarToolHiddenMap?.[DrawState.BlurFreeDraw]
    ) {
        mainToolbarButton = blurFreeDrawButton;
    }

    if (
        customToolbarToolHiddenMap?.[DrawState.Blur] &&
        customToolbarToolHiddenMap?.[DrawState.BlurFreeDraw]
    ) {
        mainToolbarButton = <></>;
    }

    return (
        <>
            <ToolbarPopover
                trigger={
                    !customToolbarToolHiddenMap?.[DrawState.Blur] &&
                    !customToolbarToolHiddenMap?.[DrawState.BlurFreeDraw]
                        ? 'hover'
                        : []
                }
                content={
                    <Flex align="center" gap={token.paddingXS} className="popover-toolbar">
                        {blurButton}
                        {!customToolbarToolHiddenMap?.[DrawState.BlurFreeDraw] &&
                            blurFreeDrawButton}
                    </Flex>
                }
            >
                <div>{mainToolbarButton}</div>
            </ToolbarPopover>
        </>
    );
};

export const BlurGroupTool = React.memo(BlurGroupToolCore);
