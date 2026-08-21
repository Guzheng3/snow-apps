import React from "react";
import { Button, Flex, Slider, theme } from "antd";
import { useCallback, useEffect, useState } from "react";

// 保留的预设值（与 excalidraw STROKE_WIDTH 一致：thin/bold/extraBold）
const STROKE_PRESETS = [1, 2, 4];
const STROKE_WIDTH_MAX = 20;

/**
 * 描边宽度滑块（注入 excalidraw 属性面板 pickerRenders.ChangeStrokeWidthSlider）
 * 选中矩形/椭圆/箭头/画笔/文字/序号等元素后，属性面板里显示：
 *   - 滑块：支持任意调节粗细（1-20）
 *   - 预设按钮：保留原来的 1 / 2 / 4 三个预设
 * onChange 由 excalidraw 的 updateData 处理（自动应用到选中元素或当前工具）
 */
export const StrokeWidthSlider: React.FC<{
	value: number | null;
	onChange: (value: number) => void;
}> = ({ value, onChange }) => {
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

	return (
		<Flex
			align="center"
			gap={4}
			style={{ padding: "0 6px", minWidth: 160 }}
			title="描边宽度"
		>
			<Slider
				min={1}
				max={STROKE_WIDTH_MAX}
				step={1}
				value={width}
				onChange={(v) => setWidth(v)}
				onChangeComplete={(v) => applyWidth(v)}
				style={{ width: 96, margin: 0 }}
			/>
			<span
				style={{
					fontSize: 10,
					color: token.colorTextSecondary,
					minWidth: 16,
					textAlign: "center",
				}}
			>
				{width}
			</span>
			{STROKE_PRESETS.map((p) => (
				<Button
					key={p}
					size="small"
					type={width === p ? "primary" : "text"}
					onClick={() => applyWidth(p)}
					style={{ fontSize: 10, padding: "0 5px", lineHeight: 1.4 }}
				>
					{p}
				</Button>
			))}
		</Flex>
	);
};
