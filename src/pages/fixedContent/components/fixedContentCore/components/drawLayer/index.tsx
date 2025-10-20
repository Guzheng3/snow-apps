import type { NormalizedZoomValue } from "@mg-chao/excalidraw/types";
import {
	getCurrentWindow,
	type PhysicalPosition,
	type PhysicalSize,
} from "@tauri-apps/api/window";
import { theme } from "antd";
import type React from "react";
import {
	Suspense,
	useCallback,
	useEffect,
	useImperativeHandle,
	useMemo,
	useRef,
	useState,
} from "react";
import {
	getMonitorsBoundingBox,
	type MonitorBoundingBox,
} from "@/commands/core";
import { DrawCore } from "@/components/drawCore";
import { withCanvasHistory } from "@/components/drawCore/components/historyContext";
import {
	type DrawCoreActionType,
	DrawCoreContext,
	type DrawCoreContextValue,
	DrawStatePublisher,
	ExcalidrawEventPublisher,
} from "@/components/drawCore/extra";
import { withStatePublisher } from "@/hooks/useStatePublisher";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import { EnableKeyEventPublisher } from "@/pages/draw/components/drawToolbar/components/keyEventWrap/extra";
import {
	DrawContext,
	type DrawContextType,
} from "@/pages/fullScreenDraw/extra";
import type { ElementRect } from "@/types/commands/screenshot";
import { DrawState } from "@/types/draw";
import { MousePosition } from "@/utils/mousePosition";
import { zIndexs } from "@/utils/zIndex";
import type { FixedContentWindowSize } from "../..";
import {
	BOX_SHADOW_WIDTH,
	FixedContentCoreDrawToolbar,
	type FixedContentCoreDrawToolbarActionType,
} from "./toolbar";

export type FixedContentCoreDrawActionType = {
	getToolbarSize: () => { width: number; height: number };
	getDrawMenuSize: () => { width: number; height: number };
	getCanvas: () => HTMLCanvasElement | null;
};

const DRAW_MENU_WIDTH = 200;
const DRAW_MENU_HEIGHT = 300;

const DrawLayerCore: React.FC<{
	actionRef: React.RefObject<FixedContentCoreDrawActionType | undefined>;
	documentSize: FixedContentWindowSize;
	scaleInfo: {
		x: number;
		y: number;
	};
	disabled?: boolean;
	hidden?: boolean;
	onConfirm: () => void;
}> = ({ actionRef, documentSize, scaleInfo, disabled, hidden, onConfirm }) => {
	const { token } = theme.useToken();

	const drawToolbarActionRef = useRef<
		FixedContentCoreDrawToolbarActionType | undefined
	>(undefined);
	const drawCoreActionRef = useRef<DrawCoreActionType | undefined>(undefined);
	const [, setEnableKeyEvent] = useStateSubscriber(
		EnableKeyEventPublisher,
		undefined,
	);

	const [excalidrawReady, setExcalidrawReady] = useState(false);
	useEffect(() => {
		if (!disabled) {
			setExcalidrawReady(true);
		}
	}, [disabled]);

	const mousePositionRef = useRef<MousePosition | undefined>(undefined);
	useEffect(() => {
		const onMouseMove = (ev: MouseEvent) => {
			mousePositionRef.current = new MousePosition(ev.clientX, ev.clientY);
		};

		document.addEventListener("mousemove", onMouseMove);

		return () => {
			document.removeEventListener("mousemove", onMouseMove);
		};
	}, []);

	const getDrawMenuSize = useCallback(() => {
		return {
			width: DRAW_MENU_WIDTH + 3 * 2,
			height: DRAW_MENU_HEIGHT + 3 * 2 + 36,
		};
	}, []);

	const [toolbarEnable, setToolbarEnable] = useState(false);
	const currentindowContext = useRef<
		| {
				monitorBounds: MonitorBoundingBox | undefined;
				windowSize: PhysicalSize;
				windowPosition: PhysicalPosition;
		  }
		| undefined
	>(undefined);
	const activeToolbar = useCallback(async () => {
		const appWindow = getCurrentWindow();
		const [windowSize, windowPosition] = await Promise.all([
			appWindow.outerSize(),
			appWindow.outerPosition(),
		]);
		const points = [
			[
				Math.round(windowPosition.x + windowSize.width / 2),
				Math.round(windowPosition.y + windowSize.height / 2),
			],
			[windowPosition.x, windowPosition.y],
			[
				windowPosition.x + windowSize.width,
				windowPosition.y + windowSize.height,
			],
			[windowPosition.x + windowSize.width, windowPosition.y],
			[windowPosition.x, windowPosition.y + windowSize.height],
		];

		let monitorBounds: MonitorBoundingBox | undefined;
		for (const point of points) {
			const bounds = await getMonitorsBoundingBox(
				{
					min_x: point[0],
					min_y: point[1],
					max_x: point[0],
					max_y: point[1],
				},
				false,
			);
			if (bounds.monitor_rect_list.length > 0) {
				monitorBounds = bounds;
				break;
			}
		}

		currentindowContext.current = {
			monitorBounds: monitorBounds,
			windowSize,
			windowPosition,
		};

		setToolbarEnable(true);
	}, []);

	const drawCoreContextValue = useMemo<DrawCoreContextValue>(() => {
		return {
			getLimitRect: () => {
				return {
					min_x: 0,
					min_y: 0,
					max_x: documentSize.width * window.devicePixelRatio,
					max_y: documentSize.height * window.devicePixelRatio,
				};
			},
			getDevicePixelRatio: () => {
				return window.devicePixelRatio;
			},
			getBaseOffset: (limitRect: ElementRect, devicePixelRatio: number) => {
				return {
					x: limitRect.max_x / devicePixelRatio + token.marginXXS,
					y: limitRect.min_x / devicePixelRatio + 3,
				};
			},
			getAction: () => {
				return drawCoreActionRef.current;
			},
			getMousePosition: () => {
				return mousePositionRef.current;
			},
			calculatedBoundaryRect: (
				rect: ElementRect,
				toolbarWidth: number,
				toolbarHeight: number,
			) => {
				if (
					!currentindowContext.current ||
					!currentindowContext.current.monitorBounds
				) {
					return {
						min_x: rect.min_x + BOX_SHADOW_WIDTH,
						min_y: rect.min_y + BOX_SHADOW_WIDTH,
						max_x: rect.max_x - BOX_SHADOW_WIDTH,
						max_y: rect.max_y - BOX_SHADOW_WIDTH,
					};
				}

				// 获取窗口相对显示器的左上角
				let contentMinX = Math.max(
					currentindowContext.current.windowPosition.x,
					currentindowContext.current.monitorBounds.rect.min_x,
				);
				let contentMinY = Math.max(
					currentindowContext.current.windowPosition.y,
					currentindowContext.current.monitorBounds.rect.min_y,
				);
				// 获取窗口相对显示器的右下角
				let contentMaxX = Math.min(
					currentindowContext.current.windowPosition.x +
						currentindowContext.current.windowSize.width,
					currentindowContext.current.monitorBounds.rect.max_x,
				);
				let contentMaxY = Math.min(
					currentindowContext.current.windowPosition.y +
						currentindowContext.current.windowSize.height,
					currentindowContext.current.monitorBounds.rect.max_y,
				);

				const toolbarPhysicalWidth = toolbarWidth * window.devicePixelRatio + 9;
				const toolbarPhysicalHeight =
					toolbarHeight * window.devicePixelRatio + 9;
				if (contentMaxX - contentMinX < toolbarPhysicalWidth) {
					if (
						contentMaxX === currentindowContext.current.monitorBounds.rect.max_x
					) {
						contentMaxX = contentMinX + toolbarPhysicalWidth;
					} else {
						contentMinX = contentMaxX - toolbarPhysicalWidth;
					}
				}

				if (contentMaxY - contentMinY < toolbarPhysicalHeight) {
					if (
						contentMaxY === currentindowContext.current.monitorBounds.rect.max_y
					) {
						contentMaxY = contentMinY + toolbarPhysicalHeight;
					} else {
						contentMinY = contentMaxY - toolbarPhysicalHeight;
					}
				}

				const minXOffset =
					(contentMinX - currentindowContext.current.windowPosition.x) /
					window.devicePixelRatio;
				const minYOffset =
					(contentMinY - currentindowContext.current.windowPosition.y) /
					window.devicePixelRatio;
				const maxXOffset =
					document.body.clientWidth -
					(contentMaxX - currentindowContext.current.windowPosition.x) /
						window.devicePixelRatio;
				const maxYOffset =
					document.body.clientHeight -
					(contentMaxY - currentindowContext.current.windowPosition.y) /
						window.devicePixelRatio;

				return {
					min_x: rect.min_x + minXOffset + BOX_SHADOW_WIDTH,
					min_y: rect.min_y + minYOffset + BOX_SHADOW_WIDTH,
					max_x: rect.max_x - maxXOffset - BOX_SHADOW_WIDTH,
					max_y: rect.max_y - maxYOffset - BOX_SHADOW_WIDTH,
				};
			},
		};
	}, [documentSize.height, documentSize.width, token.marginXXS]);

	useImperativeHandle(actionRef, () => {
		return {
			getToolbarSize: () => {
				return (
					drawToolbarActionRef.current?.getSize() ?? {
						width: 0,
						height: 0,
					}
				);
			},
			getDrawMenuSize,
			getCanvas: () => {
				if (
					drawCoreActionRef.current?.getExcalidrawAPI()?.getSceneElements()
						.length === 0
				) {
					return null;
				}

				return drawCoreActionRef.current?.getCanvas() ?? null;
			},
		};
	}, [getDrawMenuSize]);

	const drawContextValue = useMemo<DrawContextType>(() => {
		return {
			getDrawCoreAction: () => drawCoreActionRef.current,
			setTool: (drawState: DrawState) => {
				drawToolbarActionRef.current?.setTool(drawState);
			},
		};
	}, []);

	const onMouseEvent = useCallback(
		(event: React.MouseEvent<HTMLDivElement>) => {
			event.preventDefault();
			event.stopPropagation();
		},
		[],
	);

	useEffect(() => {
		setEnableKeyEvent(!(disabled ?? false));
	}, [disabled, setEnableKeyEvent]);

	useEffect(() => {
		if (!drawCoreActionRef.current) {
			return;
		}

		const appState = drawCoreActionRef.current?.getAppState();
		if (!appState) {
			return;
		}

		drawCoreActionRef.current?.updateScene({
			appState: {
				scrollX: appState.scrollX,
				scrollY: appState.scrollY,
				zoom: {
					value: (scaleInfo.x / 100) as NormalizedZoomValue,
				},
			},
			captureUpdate: "NEVER",
		});
	}, [scaleInfo.x]);

	useEffect(() => {
		if (disabled) {
			drawCoreActionRef.current?.finishDraw();
			drawToolbarActionRef.current?.setTool(DrawState.Select);
			setToolbarEnable(false);
		} else {
			activeToolbar();
		}
	}, [disabled, activeToolbar]);

	return (
		<DrawContext.Provider value={drawContextValue}>
			<div
				className="fixed-content-draw-layer"
				onMouseDown={onMouseEvent}
				onClick={onMouseEvent}
				onDoubleClick={onMouseEvent}
			>
				<DrawCoreContext.Provider value={drawCoreContextValue}>
					<Suspense>
						<DrawCore
							actionRef={drawCoreActionRef}
							zIndex={zIndexs.Draw_DrawCacheLayer}
							layoutMenuZIndex={zIndexs.Draw_ExcalidrawToolbar}
							appStateStorageKey={"fixed-content-draw-layer"}
						/>
					</Suspense>
					<FixedContentCoreDrawToolbar
						actionRef={drawToolbarActionRef}
						disabled={!toolbarEnable}
						documentSize={documentSize}
						onConfirm={() => {
							onConfirm();
						}}
					/>
				</DrawCoreContext.Provider>

				<style jsx>{`
                    .fixed-content-draw-layer {
                        position: absolute;
                        top: 0;
                        left: 0;
                        width: ${documentSize.width}px;
                        height: ${documentSize.height}px;
                        opacity: ${!excalidrawReady || hidden ? 0 : 1};
                        pointer-events: ${disabled ? "none" : "auto"};
                    }

                    .fixed-content-draw-layer :global(.Island.App-menu__left) {
                        max-height: ${Math.max(
													documentSize.height + 19,
													DRAW_MENU_HEIGHT + 15,
												)}px !important;
                    }
                `}</style>
			</div>
		</DrawContext.Provider>
	);
};

export const DrawLayer = withCanvasHistory(
	withStatePublisher(
		DrawLayerCore,
		DrawStatePublisher,
		ExcalidrawEventPublisher,
		EnableKeyEventPublisher,
	),
);
