import { Button, Flex, theme } from 'antd';
import { useIntl } from 'react-intl';
import { useState, useCallback, useContext, useMemo } from 'react';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/app/fullScreenDraw/components/drawCore/extra';
import { getButtonTypeByState } from '../../../extra';
import { ArrowIcon, LineIcon } from '@/components/icons';
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

const ArrowToolCore: React.FC<{
    customToolbarToolHiddenMap: Partial<Record<DrawState, boolean>> | undefined;
    onToolClickAction: (tool: DrawState) => void;
    disable: boolean;
}> = ({ customToolbarToolHiddenMap, onToolClickAction, disable }) => {
    const intl = useIntl();
    const { token } = theme.useToken();

    const { updateAppSettings } = useContext(AppSettingsActionContext);

    const [lastArrowTool, setLastArrowTool] = useState<DrawState>(DrawState.Arrow);
    useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (settings: AppSettingsData) => {
                setLastArrowTool(settings[AppSettingsGroup.Cache].lastArrowTool);
            },
            [setLastArrowTool],
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

    const updateLastArrowTool = useCallback(
        (value: DrawState) => {
            updateAppSettings(
                AppSettingsGroup.Cache,
                { lastArrowTool: value },
                true,
                true,
                false,
                true,
                false,
            );
        },
        [updateAppSettings],
    );

    const arrowButton = useMemo(() => {
        return (
            <ToolButton
                hidden={customToolbarToolHiddenMap?.[DrawState.Arrow]}
                componentKey={KeyEventKey.ArrowTool}
                icon={<ArrowIcon style={{ fontSize: '0.83em' }} />}
                drawState={DrawState.Arrow}
                disable={disable}
                key="arrow"
                onClick={() => {
                    onToolClickAction(DrawState.Arrow);
                    updateLastArrowTool(DrawState.Arrow);
                }}
            />
        );
    }, [disable, customToolbarToolHiddenMap, onToolClickAction, updateLastArrowTool]);

    const lineButton = useMemo(() => {
        return (
            <Button
                icon={<LineIcon style={{ fontSize: '1.15em', height: '1em' }} />}
                title={intl.formatMessage({ id: 'draw.lineTool' })}
                type={getButtonTypeByState(drawState === DrawState.Line)}
                key="line"
                onClick={() => {
                    onToolClickAction(DrawState.Line);
                    updateLastArrowTool(DrawState.Line);
                }}
                disabled={disable}
            />
        );
    }, [disable, drawState, intl, onToolClickAction, updateLastArrowTool]);

    let mainToolbarButton = customToolbarToolHiddenMap?.[DrawState.Arrow]
        ? lineButton
        : arrowButton;
    if (lastArrowTool === DrawState.Arrow && !customToolbarToolHiddenMap?.[DrawState.Arrow]) {
        mainToolbarButton = arrowButton;
    } else if (lastArrowTool === DrawState.Line && !customToolbarToolHiddenMap?.[DrawState.Line]) {
        mainToolbarButton = lineButton;
    }

    if (
        customToolbarToolHiddenMap?.[DrawState.Arrow] &&
        customToolbarToolHiddenMap?.[DrawState.Line]
    ) {
        mainToolbarButton = <></>;
    }

    return (
        <>
            <ToolbarPopover
                trigger={
                    !customToolbarToolHiddenMap?.[DrawState.Arrow] &&
                    !customToolbarToolHiddenMap?.[DrawState.Line]
                        ? 'hover'
                        : []
                }
                content={
                    <Flex align="center" gap={token.paddingXS} className="popover-toolbar">
                        {arrowButton}
                        {!customToolbarToolHiddenMap?.[DrawState.Line] && lineButton}
                    </Flex>
                }
            >
                <div>{mainToolbarButton}</div>
            </ToolbarPopover>
        </>
    );
};

export const ArrowTool = React.memo(ArrowToolCore);
