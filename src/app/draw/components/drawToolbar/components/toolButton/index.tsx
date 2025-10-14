import { DrawState } from '@/app/fullScreenDraw/components/drawCore/extra';
import { KeyEventWrap } from '@/app/draw/components/drawToolbar/components/keyEventWrap';
import React, { useCallback, useState } from 'react';
import { Button } from 'antd';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { getButtonTypeByState } from '../../extra';
import { KeyEventKey } from '../keyEventWrap/extra';
import { HotkeysScope } from '@/components/globalLayoutExtra';

const ToolButtonCore: React.FC<{
    hidden?: boolean;
    componentKey?: KeyEventKey;
    icon: React.ReactNode;
    onClick: () => void;
    drawState: DrawState;
    extraDrawState?: DrawState[];
    enableState?: boolean;
    disable?: boolean;
    confirmTip?: React.ReactNode;
    hotkeyScope?: HotkeysScope;
    buttonProps?: React.ComponentProps<typeof Button>;
}> = ({
    hidden,
    componentKey,
    icon,
    onClick,
    drawState: propDrawState,
    extraDrawState,
    enableState,
    disable,
    confirmTip,
    hotkeyScope,
    buttonProps,
}) => {
    const [buttonType, setButtonType] = useState(getButtonTypeByState(false));
    const updateButtonType = useCallback(
        (drawState: DrawState) => {
            setButtonType(
                getButtonTypeByState(
                    drawState === propDrawState ||
                        enableState ||
                        (extraDrawState?.includes(drawState) ?? false),
                ),
            );
        },
        [propDrawState, enableState, extraDrawState],
    );

    useStateSubscriber(DrawStatePublisher, updateButtonType);

    const buttonDom = (
        <Button
            style={{
                display: hidden ? 'none' : undefined,
            }}
            {...buttonProps}
            icon={icon}
            type={buttonType}
            onClick={onClick}
            disabled={disable}
            key={componentKey}
        />
    );

    if (!componentKey) {
        return buttonDom;
    }

    return (
        <KeyEventWrap
            onKeyUpEventPropName="onClick"
            componentKey={componentKey}
            confirmTip={confirmTip}
            enable={disable ? false : undefined}
            hotkeyScope={hotkeyScope}
        >
            {buttonDom}
        </KeyEventWrap>
    );
};

export const ToolButton = React.memo(ToolButtonCore);
