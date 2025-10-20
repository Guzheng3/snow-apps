import type { ColorInstance } from "color";
import { createContext, useContext } from "react";
import type { DrawState } from "@/types/draw";
import type { MousePosition } from "@/utils/mousePosition";
import type { DrawCoreActionType } from "./components/drawCore/extra";

export type DrawContextType = {
	getDrawCoreAction: () => DrawCoreActionType | undefined;
	setTool: (tool: DrawState) => void;
	enableColorPicker?: boolean;
	pickColor?: (mousePosition: MousePosition) => Promise<string | undefined>;
	getColorPickerCurrentColor?: () => ColorInstance | undefined;
	setColorPickerForceEnable?: (forceEnable: boolean) => void;
	getPopupContainer?: ((triggerNode: HTMLElement) => HTMLElement) | undefined;
};

export const DrawContext = createContext<DrawContextType>({
	getDrawCoreAction: () => undefined,
	setTool: () => {},
	enableColorPicker: false,
	pickColor: undefined,
	setColorPickerForceEnable: () => {},
	getPopupContainer: undefined,
});

export const useDrawContext = () => {
	const context = useContext(DrawContext);
	return context;
};
