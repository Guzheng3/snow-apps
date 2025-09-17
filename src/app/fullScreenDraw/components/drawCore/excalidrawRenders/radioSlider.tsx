import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { Slider } from 'antd';
import { useGetPopupContainer } from '.';
import { useChangeFontSizeProps } from './radioSelection';
import { useMemo } from 'react';

export const STROKE_WIDTH_MAX_VALUE = 32;
export const FONT_SIZE_MAX_VALUE = 128;

export const ChangeStrokeWidthSlider: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['ChangeStrokeWidthSlider']
> = ({ value, onChange, group }) => {
    const maxValue = useMemo(() => {
        if (group === 'stroke-width') {
            return STROKE_WIDTH_MAX_VALUE;
        } else if (group === 'font-size') {
            return FONT_SIZE_MAX_VALUE;
        }

        return STROKE_WIDTH_MAX_VALUE;
    }, [group]);

    // const marks = useMemo<React.ComponentProps<typeof Slider>['marks']>(() => {
    //     return {
    //         [1]: '1',
    //         [maxValue]: `${maxValue}`,
    //     };
    // }, [maxValue]);

    const getPopupContainer = useGetPopupContainer();

    return (
        <Slider
            min={1}
            max={maxValue}
            step={1}
            value={value ?? 1}
            onChange={onChange}
            tooltip={{ getPopupContainer }}
        />
    );
};

export const ChangeFontSizeSlider: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['ChangeFontSizeSlider']
> = ({ value, onChange, group, options }) => {
    const { propsRef } = useChangeFontSizeProps(true, {
        group,
        options: options,
        value: value as unknown as number,
        onChange: (value) => onChange(value as unknown as number),
    });

    return (
        <ChangeStrokeWidthSlider
            value={value}
            onChange={(value) => {
                if (!propsRef.current || !('onChange' in propsRef.current)) {
                    return;
                }

                propsRef.current.onChange(value as unknown as number);
            }}
            group={group}
            options={options}
        />
    );
};
