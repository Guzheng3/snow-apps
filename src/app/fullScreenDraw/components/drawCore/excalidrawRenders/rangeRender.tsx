import { Slider, theme } from 'antd';
import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import React from 'react';
import { useGetPopupContainer } from '.';

const RangeRenderCore: React.FC<
    Parameters<
        NonNullable<NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['rangeRender']>
    >[0]
> = (props) => {
    const { token } = theme.useToken();

    const getPopupContainer = useGetPopupContainer();

    return (
        <Slider
            style={{ marginTop: token.marginSM, marginBottom: token.marginXXS }}
            min={props.min}
            max={props.max}
            step={props.step}
            value={props.value}
            onChange={props.onChange}
            tooltip={{ placement: 'bottom', getPopupContainer }}
        />
    );
};

const RangeRenderMemo = React.memo(RangeRenderCore);

export const rangeRender: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['rangeRender']
> = (props) => {
    return <RangeRenderMemo {...props} />;
};
