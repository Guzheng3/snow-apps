import { Button } from "antd";
import { useCallback, useState } from "react";
import { useIntl } from "react-intl";
import { DrawStatePublisher } from "@/components/drawCore/extra";
import {
} from "@/components/icons";
import {
	} from "@/constants/pluginService";
import { usePluginServiceContext } from "@/contexts/pluginServiceContext";
import { useStateSubscriber } from "@/hooks/useStateSubscriber";
import {
	type AppOcrResult,
	OcrResultType,
} from "@/pages/fixedContent/components/ocrResult";
import { DrawState } from "@/types/draw";
import { SubTools } from "../../subTools";
import { OcrToolModalSettings } from "./components/ocrToolModalSettings";

export const isOcrTool = (drawState: DrawState) => {
	return (
		drawState === DrawState.OcrDetect
	);
};

const OcrTool: React.FC<{
	onSwitchOcrResult: (ocrResultType: OcrResultType) => void;
	onConvertImageToHtml: () => void;
	onConvertImageToMarkdown: () => void;
	currentOcrResult:
		| (AppOcrResult & { ocrResultType: OcrResultType })
		| undefined;
	ocrResult: AppOcrResult | undefined;
	visionModelHtmlResult: AppOcrResult | undefined;
	visionModelHtmlLoading: boolean;
	visionModelMarkdownResult: AppOcrResult | undefined;
	visionModelMarkdownLoading: boolean;
}> = ({
	onSwitchOcrResult,
	onConvertImageToHtml,
	onConvertImageToMarkdown,
	currentOcrResult,
	ocrResult,
	visionModelHtmlResult,
	visionModelHtmlLoading,
	visionModelMarkdownResult,
	visionModelMarkdownLoading,
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
					? [
							<Button
								disabled={!currentOcrResult}
								onClick={() => {
									if (ocrResult) {
										 else {
											
										}
									}
								}}
								type={
									false
										? "primary"
										: "text"
								}
								title={""}
								key="translate"
							/>,
						]
					: []),
									<OcrToolModalSettings
					key="ocrToolModalSettings"
					onFinish={async () => {
						
						return;
					}}
				/>,
			]}
		/>
	);
};

export default OcrTool;
