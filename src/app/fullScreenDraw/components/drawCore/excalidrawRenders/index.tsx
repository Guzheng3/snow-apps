import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { layoutMenuRender } from './layoutMenuRender';
import { colorPickerTopPickesButtonRender } from './colorPickerTopPickesButtonRender';
import { colorPickerPopoverRender } from './colorPickerPopoverRender';
import { ButtonIcon } from './buttonIcon';
import { buttonIconSelectRadioRender } from './buttonIconSelectRadioRender';
import { rangeRender } from './rangeRender';
import { layerButtonRender } from './layerButtonRender';
import { ButtonList } from './buttonList';
import { RadioSelection } from './radioSelection';
import SerialNumberEditor from './serialNumberEditor';
import SubToolEditor from './SubToolEditor';
import { useCallback, useEffect, useRef } from 'react';
import { ChangeFontSizeSlider, ChangeStrokeWidthSlider } from './radioSlider';

export const useGetPopupContainer = () => {
    const containerRef = useRef<HTMLElement>(null);
    useEffect(() => {
        containerRef.current = document.getElementById('layout-menu-render') ?? document.body;
    }, []);

    return useCallback(() => {
        return containerRef.current ?? document.body;
    }, []);
};

export const generatePickerRenders: (
    enableSliderChangeWidth: boolean,
) => ExcalidrawPropsCustomOptions['pickerRenders'] = (enableSliderChangeWidth) => {
    return {
        colorPickerTopPickesButtonRender,
        colorPickerPopoverRender,
        buttonIconSelectRadioRender,
        CustomButtonIcon: ButtonIcon,
        RadioSelection: RadioSelection as unknown as NonNullable<
            ExcalidrawPropsCustomOptions['pickerRenders']
        >['RadioSelection'],
        rangeRender,
        layerButtonRender,
        elementStrokeColors: ['#1e1e1e', '#f5222d', '#52c41a', '#1677ff', '#faad14'],
        elementBackgroundColors: ['transparent', '#ffccc7', '#d9f7be', '#bae0ff', '#fff1b8'],
        ButtonList: ButtonList,
        SerialNumberEditor,
        SubToolEditor,
        ChangeStrokeWidthSlider: enableSliderChangeWidth ? ChangeStrokeWidthSlider : undefined,
        ChangeFontSizeSlider: enableSliderChangeWidth ? ChangeFontSizeSlider : undefined,
    };
};

export const layoutRenders: ExcalidrawPropsCustomOptions['layoutRenders'] = {
    menuRender: layoutMenuRender,
};
