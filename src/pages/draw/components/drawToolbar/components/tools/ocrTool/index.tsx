import { Button } from "antd";
import { useCallback, useRef, useState } from "react";
import { useIntl } from "react-intl";
import { OcrTranslateIcon } from "@/components/icons";
import { useStateRef } from "@/hooks/useStateRef";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import {
	DrawEvent,
	type DrawEventParams,
	DrawEventPublisher,
} from "@/pages/draw/extra";
import { DrawStatePublisher } from "@/pages/fullScreenDraw/components/drawCore/extra";
import type { OcrDetectResult } from "@/types/commands/ocr";
import { DrawState } from "@/types/draw";
import { SubTools } from "../../subTools";
import {
	ModalTranslator,
	type ModalTranslatorActionType,
} from "./components/modalTranslator";

export const isOcrTool = (drawState: DrawState) => {
	return (
		drawState === DrawState.OcrDetect || drawState === DrawState.OcrTranslate
	);
};

const OcrTool: React.FC<{
	onReplace: (result: OcrDetectResult, ignoreScale?: boolean) => void;
}> = ({ onReplace }) => {
	const intl = useIntl();

	const modalTranslatorActionRef = useRef<ModalTranslatorActionType>(undefined);

	const [enabled, setEnabled] = useState(false);
	const [ocrResult, setOcrResult, ocrResultRef] = useStateRef<
		OcrDetectResult | undefined
	>(undefined);
	const getOcrResult = useCallback(() => {
		return ocrResultRef.current;
	}, [ocrResultRef]);

	const [getDrawState] = useStateSubscriber(DrawStatePublisher, undefined);
	useStateSubscriber(
		DrawStatePublisher,
		useCallback(
			(drawState: DrawState) => {
				if (isOcrTool(drawState)) {
					setEnabled(true);
				} else {
					setEnabled(false);
					setOcrResult(undefined);
				}
			},
			[setOcrResult],
		),
	);
	useStateSubscriber(
		DrawEventPublisher,
		useCallback(
			(drawEvent: DrawEventParams) => {
				if (drawEvent?.event === DrawEvent.OcrDetect) {
					setOcrResult(drawEvent.params.result);

					// 自动进行翻译
					if (getDrawState() === DrawState.OcrTranslate) {
						modalTranslatorActionRef.current?.startTranslate();
					}
				}
			},
			[getDrawState, setOcrResult],
		),
	);

	if (!enabled) {
		return null;
	}

	return (
		<>
			<SubTools
				buttons={[
					<Button
						disabled={!ocrResult}
						onClick={() => {
							modalTranslatorActionRef.current?.startTranslate();
						}}
						icon={<OcrTranslateIcon />}
						title={intl.formatMessage({ id: "draw.ocrDetect.translate" })}
						type={"text"}
						key="translate"
					/>,
				]}
			/>
			<ModalTranslator
				actionRef={modalTranslatorActionRef}
				getOcrResult={getOcrResult}
				onReplace={onReplace}
			/>
		</>
	);
};

export default OcrTool;
