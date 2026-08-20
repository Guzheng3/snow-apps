import { Button, Flex, Slider, theme } from "antd";
import { useCallback, useContext, useEffect, useState } from "react";
import { DrawContext } from "@/pages/draw/types";

// 保留的预设值（原 strokeWidthList）
const STROKE_PRESETS = [1, 2, 4];
const STROKE_WIDTH_MAX = 20;

/**
 * 描边宽度滑块：支持滑块调节粗细，同时保留预设值快捷按钮
 */
export const StrokeWidthSlider: React.FC = () => {
	const { token } = theme.useToken();
	const { drawLayerActionRef } = useContext(DrawContext);
	const [width, setWidth] = useState(1);

	useEffect(() => {
		const appState = drawLayerActionRef.current?.getAppState();
		const current = appState?.currentItemStrokeWidth ?? 1;
		setWidth(current);
	}, [drawLayerActionRef]);

	const applyWidth = useCallback(
		(value: number) => {
			const layer = drawLayerActionRef.current;
			if (!layer) {
				return;
			}
			const appState = layer.getAppState();
			if (!appState) {
				return;
			}
			const sceneElements =
				layer.getExcalidrawAPI()?.getSceneElements() ?? [];
			const selectedIds = appState.selectedElementIds;
			const selectedIdsArr = Object.keys(selectedIds).filter(
				(id) => selectedIds[id],
			);
			if (selectedIdsArr.length === 1) {
				const selected = sceneElements.find(
					(el) => el.id === selectedIdsArr[0],
				);
				if (selected && "strokeWidth" in selected) {
					layer.updateScene({
						elements: sceneElements.map((el) =>
							el.id === selected.id
								? { ...el, strokeWidth: value }
								: el,
						),
						captureUpdate: "IMMEDIATELY",
					});
					setWidth(value);
					return;
				}
			}
			layer.updateScene({
				appState: { ...appState, currentItemStrokeWidth: value },
			});
			setWidth(value);
		},
		[drawLayerActionRef],
	);

	return (
		<Flex
			align="center"
			gap={4}
			style={{ padding: "0 6px", minWidth: 150 }}
			title="描边宽度"
		>
			<Slider
				min={1}
				max={STROKE_WIDTH_MAX}
				step={1}
				value={width}
				onChange={(v) => setWidth(v)}
				onChangeComplete={(v) => applyWidth(v)}
				style={{ width: 90, margin: 0 }}
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
