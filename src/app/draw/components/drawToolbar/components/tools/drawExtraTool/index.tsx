import { Button, Flex, Popover, theme } from 'antd';
import { useIntl } from 'react-intl';
import { useState, useCallback, useContext } from 'react';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/app/fullScreenDraw/components/drawCore/extra';
import { getButtonTypeByState } from '../../../extra';
import { WatermarkIcon } from '@/components/icons';
import {
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
} from '@/app/contextWrap';
import { WatermarkTool } from './components/watermarkTool';

export const DrawExtraTool: React.FC<{
    onToolClick: (tool: DrawState) => void;
}> = ({ onToolClick }) => {
    const intl = useIntl();
    const { token } = theme.useToken();

    const { updateAppSettings } = useContext(AppSettingsActionContext);

    const [lastDrawExtraTool, setLastDrawExtraTool] = useState<DrawState>(DrawState.Idle);
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

    const watermarkButton = (
        <Button
            icon={<WatermarkIcon />}
            title={intl.formatMessage({ id: 'draw.watermarkTool' })}
            type={getButtonTypeByState(drawState === DrawState.Watermark)}
            key="watermark"
            onClick={() => {
                onToolClick(DrawState.Watermark);
                updateLastDrawExtraTool(DrawState.Watermark);
            }}
        />
    );

    let mainToolbarButton = watermarkButton;

    if (lastDrawExtraTool === DrawState.Watermark) {
        mainToolbarButton = watermarkButton;
    }

    return (
        <>
            <Popover
                trigger="hover"
                content={
                    <Flex align="center" gap={token.paddingXS} className="popover-toolbar">
                        {watermarkButton}
                    </Flex>
                }
            >
                <div>{mainToolbarButton}</div>
            </Popover>

            <WatermarkTool />
        </>
    );
};
