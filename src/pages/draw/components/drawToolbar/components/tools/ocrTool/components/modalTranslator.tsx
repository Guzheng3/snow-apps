import { Modal } from "antd";
import React from "react";
import { FormattedMessage } from "react-intl";
import type { OcrDetectResult } from "@/types/commands/ocr";

export type ModalTranslatorActionType = {};

export const ModalTranslatorCore: React.FC<{
	getOcrResult: () => OcrDetectResult | undefined;
	actionRef: React.RefObject<ModalTranslatorActionType | undefined>;
	onReplace: (result: OcrDetectResult, ignoreScale?: boolean) => void;
}> = () => {
	return (
		<Modal
			width={800}
			centered
			forceRender={true}
			title={<FormattedMessage id="draw.ocrDetect.translate" />}
		></Modal>
	);
};

export const ModalTranslator = React.memo(ModalTranslatorCore);
