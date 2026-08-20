"use client";

import { Button, Flex, theme } from "antd";
import { useCallback, useContext, useMemo, useState } from "react";
import { useIntl } from "react-intl";
import { DrawStatePublisher } from "@/components/drawCore/extra";
import { HighlightIcon, WatermarkIcon } from "@/components/icons";
import {
	AppSettingsActionContext,
	AppSettingsPublisher,
} from "@/contexts/appSettingsActionContext";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import { type AppSettingsData, AppSettingsGroup } from "@/types/appSettings";
import { DrawState } from "@/types/draw";
import { getButtonTypeByState } from "../../../extra";
import { WatermarkTool } from "./components/watermarkTool";

/**
 * 水印 / 高亮：两个独立按钮，图标下方显示文字描述（间距/字体与其他工具按钮一致）
 */
const DrawExtraToolCore: React.FC<{
	customToolbarToolHiddenMap: Partial<Record<DrawState, boolean>> | undefined;
	onToolClickAction: (tool: DrawState) => void;
	disable: boolean;
}> = ({ customToolbarToolHiddenMap, onToolClickAction, disable }) => {
	const intl = useIntl();
	const { token } = theme.useToken();

	const { updateAppSettings } = useContext(AppSettingsActionContext);

	const [lastDrawExtraTool, setLastDrawExtraTool] = useState<DrawState>(
		DrawState.Watermark,
	);
	useStateSubscriber(
		AppSettingsPublisher,
		useCallback((settings: AppSettingsData) => {
			setLastDrawExtraTool(settings[AppSettingsGroup.Cache].lastDrawExtraTool);
		}, []),
	);
	const [drawState, setDrawState] = useState(DrawState.Idle);
	useStateSubscriber(
		DrawStatePublisher,
		useCallback((state: DrawState) => {
			setDrawState(state);
		}, []),
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

	// 与 ToolButton 一致的「图标 + 下方文字」布局（gap 7、fontSize 10）
	const renderLabeledButton = useCallback(
		(
			key: string,
			icon: React.ReactNode,
			label: string,
			targetState: DrawState,
			onClick: () => void,
		) => {
			return (
				<div
					className="draw-toolbar-btn-wrap"
					style={{
						display: "inline-flex",
						flexDirection: "column",
						alignItems: "center",
						gap: 7,
						lineHeight: 1,
					}}
				>
					<Button
						icon={icon}
						title={label}
						type={getButtonTypeByState(drawState === targetState)}
						key={key}
						onClick={onClick}
						disabled={disable}
					/>
					<span
						style={{
							fontSize: 10,
							lineHeight: 1.1,
							whiteSpace: "nowrap",
							overflow: "hidden",
							textOverflow: "ellipsis",
							maxWidth: 72,
							color: token.colorTextSecondary,
							pointerEvents: "none",
						}}
					>
						{label}
					</span>
				</div>
			);
		},
		[disable, drawState, token.colorTextSecondary],
	);

	const watermarkButton = useMemo(() => {
		return renderLabeledButton(
			"watermark",
			<WatermarkIcon />,
			intl.formatMessage({ id: "draw.watermarkTool" }),
			DrawState.Watermark,
			() => {
				onToolClickAction(DrawState.Watermark);
				updateLastDrawExtraTool(DrawState.Watermark);
			},
		);
	}, [intl, onToolClickAction, renderLabeledButton, updateLastDrawExtraTool]);

	const highlightButton = useMemo(() => {
		return renderLabeledButton(
			"highlight",
			<HighlightIcon />,
			intl.formatMessage({ id: "draw.highlightTool" }),
			DrawState.Highlight,
			() => {
				onToolClickAction(DrawState.Highlight);
				updateLastDrawExtraTool(DrawState.Highlight);
			},
		);
	}, [intl, onToolClickAction, renderLabeledButton, updateLastDrawExtraTool]);

	if (
		customToolbarToolHiddenMap?.[DrawState.Watermark] &&
		customToolbarToolHiddenMap?.[DrawState.Highlight]
	) {
		return (
			<>
				<WatermarkTool />
			</>
		);
	}

	return (
		<>
			<Flex align="center" gap={token.paddingXS} className="popover-toolbar">
				{!customToolbarToolHiddenMap?.[DrawState.Watermark] && watermarkButton}
				{!customToolbarToolHiddenMap?.[DrawState.Highlight] && highlightButton}
			</Flex>

			<WatermarkTool />
		</>
	);
};

export const DrawExtraTool = React.memo(DrawExtraToolCore);
