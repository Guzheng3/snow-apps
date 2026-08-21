import React, { useCallback, useContext, useMemo, useState } from "react";
import { DrawStatePublisher } from "@/components/drawCore/extra";
import { ArrowIcon, LineIcon } from "@/components/icons";
import {
	AppSettingsActionContext,
	AppSettingsPublisher,
} from "@/contexts/appSettingsActionContext";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import { type AppSettingsData, AppSettingsGroup } from "@/types/appSettings";
import { DrawToolbarKeyEventKey } from "@/types/components/drawToolbar";
import { DrawState } from "@/types/draw";
import { ToolButton } from "../../toolButton";

const ArrowToolCore: React.FC<{
	customToolbarToolHiddenMap: Partial<Record<DrawState, boolean>> | undefined;
	onToolClickAction: (tool: DrawState) => void;
	disable: boolean;
}> = ({ customToolbarToolHiddenMap, onToolClickAction, disable }) => {
	const { updateAppSettings } = useContext(AppSettingsActionContext);

	const [drawState, setDrawState] = useState(DrawState.Idle);
	useStateSubscriber(
		DrawStatePublisher,
		useCallback((state: DrawState) => {
			setDrawState(state);
		}, []),
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

	// 箭头：独立按钮，图标下方显示"箭头"文字
	const arrowButton = useMemo(() => {
		return (
			<ToolButton
				hidden={customToolbarToolHiddenMap?.[DrawState.Arrow]}
				componentKey={DrawToolbarKeyEventKey.ArrowTool}
				icon={<ArrowIcon style={{ fontSize: "0.83em" }} />}
				drawState={DrawState.Arrow}
				disable={disable}
				key="arrow"
				onClick={() => {
					onToolClickAction(DrawState.Arrow);
					updateLastArrowTool(DrawState.Arrow);
				}}
			/>
		);
	}, [
		disable,
		customToolbarToolHiddenMap,
		onToolClickAction,
		updateLastArrowTool,
	]);

	// 直线：独立按钮，图标下方显示"直线"文字（与其他工具按钮一致）
	const lineButton = useMemo(() => {
		return (
			<ToolButton
				hidden={customToolbarToolHiddenMap?.[DrawState.Line]}
				componentKey={DrawToolbarKeyEventKey.LineTool}
				icon={<LineIcon style={{ fontSize: "1.15em", height: "1em" }} />}
				drawState={DrawState.Line}
				disable={disable}
				key="line"
				onClick={() => {
					onToolClickAction(DrawState.Line);
					updateLastArrowTool(DrawState.Line);
				}}
			/>
		);
	}, [disable, customToolbarToolHiddenMap, onToolClickAction, updateLastArrowTool]);

	// 箭头与直线拆开，两个独立按钮并列显示在主工具栏
	return (
		<>
			{arrowButton}
			{lineButton}
		</>
	);
};

export const ArrowTool = React.memo(ArrowToolCore);
