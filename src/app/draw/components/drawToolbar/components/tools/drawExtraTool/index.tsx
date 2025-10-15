'use client';

import { Button, Flex, theme } from 'antd';
import { useIntl } from 'react-intl';
import { useState, useCallback, useContext, useMemo } from 'react';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/types/draw';
import { getButtonTypeByState } from '../../../extra';
import { HighlightIcon, WatermarkIcon } from '@/components/icons';
import { AppSettingsData, AppSettingsGroup } from '@/types/appSettings';
import { WatermarkTool } from './components/watermarkTool';
import { ToolbarPopover } from '@/app/draw/components/drawToolbar/components/toolbarPopover';
import {
    AppSettingsActionContext,
    AppSettingsPublisher,
} from '@/contexts/appSettingsActionContext';

export const DrawExtraTool: React.FC<{
    customToolbarToolHiddenMap: Partial<Record<DrawState, boolean>> | undefined;
    onToolClickAction: (tool: DrawState) => void;
    disable: boolean;
}> = ({ customToolbarToolHiddenMap, onToolClickAction, disable }) => {
    const intl = useIntl();
    const { token } = theme.useToken();

    const { updateAppSettings } = useContext(AppSettingsActionContext);

    const [lastDrawExtraTool, setLastDrawExtraTool] = useState<DrawState>(DrawState.Watermark);
    useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (settings: AppSettingsData) => {
                setLastDrawExtraTool(settings[AppSettingsGroup.Cache].lastDrawExtraTool);
            },
            [setLastDrawExtraTool],
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

    const updateLastDrawExtraTool = useCallback(
        (value: DrawState) => {
            updateAppSettings(
                AppSettingsGroup.Cache,
                { lastDrawExtraTool: value },
                true,
                true,
                false,
                true,
                false,
            );
        },
        [updateAppSettings],
    );

    const watermarkButton = useMemo(() => {
        return (
            <Button
                icon={<WatermarkIcon />}
                title={intl.formatMessage({ id: 'draw.watermarkTool' })}
                type={getButtonTypeByState(drawState === DrawState.Watermark)}
                key="watermark"
                onClick={() => {
                    onToolClickAction(DrawState.Watermark);
                    updateLastDrawExtraTool(DrawState.Watermark);
                }}
                disabled={disable}
            />
        );
    }, [disable, drawState, intl, onToolClickAction, updateLastDrawExtraTool]);

    const highlightButton = useMemo(() => {
        return (
            <Button
                icon={<HighlightIcon />}
                title={intl.formatMessage({ id: 'draw.highlightTool' })}
                type={getButtonTypeByState(drawState === DrawState.Highlight)}
                key="highlight"
                onClick={() => {
                    onToolClickAction(DrawState.Highlight);
                    updateLastDrawExtraTool(DrawState.Highlight);
                }}
                disabled={disable}
            />
        );
    }, [disable, drawState, intl, onToolClickAction, updateLastDrawExtraTool]);

    let mainToolbarButton = customToolbarToolHiddenMap?.[DrawState.Watermark]
        ? highlightButton
        : watermarkButton;
    if (
        lastDrawExtraTool === DrawState.Watermark &&
        !customToolbarToolHiddenMap?.[DrawState.Watermark]
    ) {
        mainToolbarButton = watermarkButton;
    } else if (
        lastDrawExtraTool === DrawState.Highlight &&
        !customToolbarToolHiddenMap?.[DrawState.Highlight]
    ) {
        mainToolbarButton = highlightButton;
    }

    if (
        customToolbarToolHiddenMap?.[DrawState.Watermark] &&
        customToolbarToolHiddenMap?.[DrawState.Highlight]
    ) {
        return null;
    }

    return (
        <>
            <ToolbarPopover
                trigger={
                    !customToolbarToolHiddenMap?.[DrawState.Watermark] &&
                    !customToolbarToolHiddenMap?.[DrawState.Highlight]
                        ? 'hover'
                        : []
                }
                content={
                    <Flex align="center" gap={token.paddingXS} className="popover-toolbar">
                        {!customToolbarToolHiddenMap?.[DrawState.Watermark] && watermarkButton}
                        {!customToolbarToolHiddenMap?.[DrawState.Highlight] && highlightButton}
                    </Flex>
                }
            >
                <div>{mainToolbarButton}</div>
            </ToolbarPopover>

            <WatermarkTool />
        </>
    );
};
