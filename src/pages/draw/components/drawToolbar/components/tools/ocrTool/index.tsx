import { Button } from "antd";
import { useCallback, useState } from "react";
import { useIntl } from "react-intl";
import { DrawStatePublisher } from "@/components/drawCore/extra";
import { OcrTranslateIcon, VisionModelHtmlIcon } from "@/components/icons";
import { PLUGIN_ID_AI_CHAT } from "@/constants/pluginService";
import { usePluginServiceContext } from "@/contexts/pluginServiceContext";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import {
	type AppOcrResult,
	OcrResultType,
} from "@/pages/fixedContent/components/ocrResult";
import type { ChatApiConfig } from "@/types/appSettings";
import { DrawState } from "@/types/draw";
import { SubTools } from "../../subTools";
import { OcrToolModalSettings } from "./components/ocrToolModalSettings";

export const isOcrTool = (drawState: DrawState) => {
	return (
		drawState === DrawState.OcrDetect || drawState === DrawState.OcrTranslate
	);
};

const OcrTool: React.FC<{
	onSwitchOcrResult: (ocrResultType: OcrResultType) => void;
	onTranslate: () => void;
	onConvertImageToHtml: () => void;
	currentOcrResult:
		| (AppOcrResult & { ocrResultType: OcrResultType })
		| undefined;
	ocrResult: AppOcrResult | undefined;
	translatedOcrResult: AppOcrResult | undefined;
	translateLoading: boolean;
	visionModelHtmlResult: AppOcrResult | undefined;
	visionModelHtmlLoading: boolean;
	visionModelList: ChatApiConfig[];
}> = ({
	onSwitchOcrResult,
	onTranslate,
	onConvertImageToHtml,
	currentOcrResult,
	ocrResult,
	translatedOcrResult,
	translateLoading,
	visionModelHtmlResult,
	visionModelHtmlLoading,
	visionModelList,
}) => {
	const intl = useIntl();

	const [enabled, setEnabled] = useState(false);

	useStateSubscriber(
		DrawStatePublisher,
		useCallback((drawState: DrawState) => {
			if (isOcrTool(drawState)) {
				setEnabled(true);
			} else {
				setEnabled(false);
			}
		}, []),
	);

	const { isReadyStatus } = usePluginServiceContext();

	if (!enabled) {
		return null;
	}

	return (
		<SubTools
			buttons={[
				<Button
					disabled={!currentOcrResult}
					loading={translateLoading}
					onClick={() => {
						if (ocrResult) {
							if (translatedOcrResult) {
								onSwitchOcrResult(
									currentOcrResult?.ocrResultType === OcrResultType.Translated
										? OcrResultType.Ocr
										: OcrResultType.Translated,
								);
							} else {
								onTranslate();
							}
						}
					}}
					type={
						currentOcrResult?.ocrResultType === OcrResultType.Translated
							? "primary"
							: "text"
					}
					icon={<OcrTranslateIcon />}
					title={intl.formatMessage({ id: "draw.ocrDetect.translate" })}
					key="translate"
				/>,
				isReadyStatus?.(PLUGIN_ID_AI_CHAT) ? (
					<Button
						disabled={visionModelList.length === 0}
						loading={visionModelHtmlLoading}
						onClick={() => {
							if (visionModelHtmlResult) {
								onSwitchOcrResult(
									currentOcrResult?.ocrResultType ===
										OcrResultType.VisionModelHtml
										? OcrResultType.Ocr
										: OcrResultType.VisionModelHtml,
								);
							} else {
								onConvertImageToHtml();
							}
						}}
						type={
							currentOcrResult?.ocrResultType === OcrResultType.VisionModelHtml
								? "primary"
								: "text"
						}
						icon={<VisionModelHtmlIcon />}
						title={intl.formatMessage({ id: "draw.ocrDetect.visionModelHtml" })}
						key="visionModelHtml"
					/>
				) : (
					false
				),
				<OcrToolModalSettings
					key="ocrToolModalSettings"
					onFinish={async () => {
						onTranslate();
						return;
					}}
				/>,
			]}
		/>
	);
};

export default OcrTool;
