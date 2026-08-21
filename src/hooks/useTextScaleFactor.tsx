import {
	type Dispatch,
	type RefObject,
	type SetStateAction,
	useCallback,
	useContext,
	useEffect,
	useState,
} from "react";
import { TextScaleFactorContext } from "@/contexts/textScaleFactorContext";
import { AppSettingsGroup } from "@/types/appSettings";
import { useAppSettingsLoad } from "./useAppSettingsLoad";
import { useStateRef } from "./useStateRef";

/**
 * 获取文本缩放比例
 */
export const useTextScaleFactor = (): [number, number, RefObject<number>] => {
	const { textScaleFactor, textScaleFactorRef, devicePixelRatio } = useContext(
		TextScaleFactorContext,
	);

	return [textScaleFactor, devicePixelRatio, textScaleFactorRef];
};

/**
 * 计算内容缩放比例
 * @param monitorScaleFactor 选中区域所在显示器的缩放比例（仅用于坐标换算，不应作用于 UI 视觉缩放）
 * @param textScaleFactor 文本缩放比例
 * @param devicePixelRatio 设备像素比
 * @returns 内容缩放比例
 *
 * 注意：UI 元素（工具栏 / 二级属性面板）始终按「窗口自身」的 DPI 在 CSS 像素空间渲染，
 * 其视觉缩放基准必须为 1。若使用「选中区域所在显示器」的 DPI（monitorScaleFactor）参与
 * transform: scale，则多显示器且 DPI 不同时 contentScale 会偏离 1，导致整个面板 / 工具栏
 * 被整体放大或缩小（预览图标被拉伸成巨大色块、内容溢出可视区）。跨显示器的坐标差异应由
 * calculatedBoundaryRect 等坐标换算逻辑处理，而不是缩放 UI 元素。
 */
const calculateContentScale = (
	_monitorScaleFactor: number,
	textScaleFactor: number,
	devicePixelRatio: number,
) => {
	if (devicePixelRatio === 0) {
		return 1;
	}

	// 窗口自身的缩放因子 = devicePixelRatio / textScaleFactor，
	// 用它作为 UI 视觉缩放基准，结果恒为 1（即按窗口自身 DPI 渲染）。
	const windowScaleFactor = devicePixelRatio / textScaleFactor;

	return (windowScaleFactor * textScaleFactor) / devicePixelRatio;
};

/**
 * 内容缩放比例
 * @returns 缩放比例
 */
export const useContentScale = (
	monitorScaleFactor: number,
	isToolbar?: boolean,
): [number, Dispatch<SetStateAction<number>>, RefObject<number>] => {
	const [textScaleFactor, devicePixelRatio] = useTextScaleFactor();
	const [contentScale, setContentScale, contentScaleRef] = useStateRef(1);

	const [uiScale, setUiScale] = useState<number>();
	const [toolbarUiScale, setToolbarUiScale] = useState<number>();

	useAppSettingsLoad(
		useCallback((settings) => {
			setUiScale(settings[AppSettingsGroup.Screenshot].uiScale);
			setToolbarUiScale(settings[AppSettingsGroup.Screenshot].toolbarUiScale);
		}, []),
		true,
	);

	useEffect(() => {
		if (!uiScale || !toolbarUiScale) {
			return;
		}

		setContentScale(
			calculateContentScale(
				monitorScaleFactor,
				textScaleFactor,
				devicePixelRatio,
			) *
				(uiScale / 100) *
				(isToolbar ? toolbarUiScale / 100 : 1),
		);
	}, [
		devicePixelRatio,
		isToolbar,
		monitorScaleFactor,
		setContentScale,
		textScaleFactor,
		toolbarUiScale,
		uiScale,
	]);

	return [contentScale, setContentScale, contentScaleRef];
};
