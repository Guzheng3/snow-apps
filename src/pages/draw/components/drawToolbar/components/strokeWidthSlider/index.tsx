import { Flex, Slider, theme } from "antd";
import React, { useCallback, useEffect, useState } from "react";

/**
 * 描边宽度（注入 excalidraw 属性面板 pickerRenders.ChangeStrokeWidthSlider）
 * 选中矩形/椭圆/箭头/画笔/文字/序号等元素后，属性面板里显示：
 *   - 上面：保留原来的三个预设（thin/bold/extraBold，RadioGroup 原生样式，紧凑）
 *   - 下面：滑块，支持任意调节粗细（1-32）
 * onChange 由 excalidraw 的 updateData 处理（自动应用到选中元素或当前工具）
 */
const STROKE_WIDTH_MAX = 32;
const FONT_SIZE_MAX = 128;

export const StrokeWidthSlider: React.FC<{
	value: number | null;
	onChange: (value: number) => void;
	options?: {
		value: number;
		text: string;
		icon: React.ReactNode;
		testId?: string;
		active?: boolean;
	}[];
	group?: string;
}> = ({ value, onChange, options, group }) => {
	const { token } = theme.useToken();
	const [width, setWidth] = useState(value ?? 1);

	// 外部 value 变化（切换选中元素/工具）时同步滑块
	useEffect(() => {
		if (value != null) {
			setWidth(value);
		}
	}, [value]);

	const applyWidth = useCallback(
		(v: number) => {
			setWidth(v);
			onChange(v);
		},
		[onChange],
	);

	const maxValue = group === "font-size" ? FONT_SIZE_MAX : STROKE_WIDTH_MAX;

	// 上面的三个预设：复刻 excalidraw 原生 RadioGroup 外观（紧凑，不撑大属性面板）
	const presetButtons = (options ?? []).map((option) => {
		const active = option.active ?? value === option.value;
		return (
			<label
				key={option.text}
				title={option.text}
				style={{
					position: "relative",
					display: "flex",
					alignItems: "center",
					justifyContent: "center",
					width: 32,
					height: 24,
					borderRadius: 8,
					color: active ? token.colorTextLightSolid : token.colorPrimary,
					background: active ? token.colorPrimary : "transparent",
					cursor: "pointer",
					userSelect: "none",
					transition: "all 75ms ease-out",
					flexShrink: 0,
				}}
			>
				<input
					type="radio"
					name={group ?? "stroke-width"}
					checked={active}
					onChange={() => applyWidth(option.value)}
					data-testid={option.testId}
					style={{
						position: "absolute",
						width: "100%",
						height: "100%",
						margin: 0,
						padding: 0,
						opacity: 0,
						cursor: "pointer",
					}}
				/>
				{option.icon}
			</label>
		);
	});

	return (
		<Flex vertical gap={8} style={{ padding: "0 6px", minWidth: 200 }}>
			{/* 上面的三个预设（保留原样） */}
			<Flex
				align="center"
				gap={4}
				style={{
					padding: 3,
					borderRadius: 10,
					background: token.colorBgContainer,
					border: `1px solid ${token.colorBorderSecondary}`,
					width: "fit-content",
				}}
			>
				{presetButtons}
			</Flex>
			{/* 下面的滑块 */}
			<Flex align="center" gap={4}>
				<Slider
					min={1}
					max={maxValue}
					step={1}
					value={width}
					onChange={(v) => setWidth(v)}
					onChangeComplete={(v) => applyWidth(v)}
					style={{ flex: 1, margin: 0 }}
				/>
				<span
					style={{
						fontSize: 11,
						color: token.colorTextSecondary,
						minWidth: 16,
						textAlign: "center",
						userSelect: "none",
					}}
				>
					{width}
				</span>
			</Flex>
		</Flex>
	);
};
