import { createContext, useContext } from 'react';
import { DrawCoreActionType, DrawState } from './components/drawCore/extra';
import { MousePosition } from '@/utils/mousePosition';
import { ColorInstance } from 'color';

export type DrawContextType = {
    getDrawCoreAction: () => DrawCoreActionType | undefined;
    setTool: (tool: DrawState) => void;
    enableColorPicker?: boolean;
    pickColor?: (mousePosition: MousePosition) => Promise<string | undefined>;
    getColorPickerCurrentColor?: () => ColorInstance | undefined;
    setColorPickerForceEnable?: (forceEnable: boolean) => void;
};

export const DrawContext = createContext<DrawContextType>({
    getDrawCoreAction: () => undefined,
    setTool: () => {},
    enableColorPicker: false,
    pickColor: undefined,
    setColorPickerForceEnable: () => {},
});

export const useDrawContext = () => {
    const context = useContext(DrawContext);
    return context;
};
