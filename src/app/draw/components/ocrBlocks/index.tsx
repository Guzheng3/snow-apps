import { useCallback, useImperativeHandle, useRef } from 'react';
import { ElementRect } from '@/commands';
import { OcrDetectResult } from '@/commands/ocr';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { CaptureBoundingBoxInfo, DrawEvent, DrawEventPublisher } from '../../extra';
import { DrawState, DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import OcrTool, { isOcrTool } from '../drawToolbar/components/tools/ocrTool';
import {
    AppOcrResult,
    covertOcrResultToText,
    OcrDetectAfterAction,
    OcrResult,
    OcrResultActionType,
} from '@/app/fixedContent/components/ocrResult';
import { zIndexs } from '@/utils/zIndex';
import { writeTextToClipboard } from '@/utils/clipboard';
import { AppSettingsGroup, AppSettingsPublisher } from '@/app/contextWrap';

export type OcrBlocksActionType = {
    init: (
        selectRect: ElementRect,
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
        canvas: HTMLCanvasElement,
        ocrResult: AppOcrResult | undefined,
    ) => Promise<void>;
    setEnable: (enable: boolean | ((enable: boolean) => boolean)) => void;
    getOcrResultAction: () => OcrResultActionType | undefined;
};

export const OcrBlocks: React.FC<{
    actionRef: React.RefObject<OcrBlocksActionType | undefined>;
    finishCapture: () => void;
}> = ({ actionRef, finishCapture }) => {
    const ocrResultActionRef = useRef<OcrResultActionType>(undefined);

    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const [getDrawState] = useStateSubscriber(
        DrawStatePublisher,
        useCallback((drawState: DrawState) => {
            ocrResultActionRef.current?.setEnable(isOcrTool(drawState));
            ocrResultActionRef.current?.clear();
        }, []),
    );
    const [, setDrawEvent] = useStateSubscriber(DrawEventPublisher, undefined);

    useImperativeHandle(
        actionRef,
        () => ({
            init: async (
                selectRect: ElementRect,
                captureBoundingBoxInfo: CaptureBoundingBoxInfo,
                canvas: HTMLCanvasElement,
                ocrResult: AppOcrResult | undefined,
            ) => {
                ocrResultActionRef.current?.init({
                    selectRect,
                    captureBoundingBoxInfo,
                    canvas,
                    ocrResult,
                });
            },
            setEnable: (enable: boolean | ((enable: boolean) => boolean)) => {
                ocrResultActionRef.current?.setEnable(enable);
            },
            getOcrResultAction: () => {
                return ocrResultActionRef.current;
            },
        }),
        [],
    );

    const onReplace = useCallback((result: OcrDetectResult, ignoreScale?: boolean) => {
        ocrResultActionRef.current?.updateOcrTextElements(result, ignoreScale);
    }, []);

    const onOcrDetect = useCallback(
        (ocrResult: OcrDetectResult) => {
            // 只在 OCR 检测时启用 OCR 后操作,截图翻译时不启用
            if (getDrawState() === DrawState.OcrDetect) {
                const ocrAfterAction =
                    getAppSettings()[AppSettingsGroup.FunctionScreenshot].ocrAfterAction;

                if (ocrAfterAction === OcrDetectAfterAction.CopyText) {
                    writeTextToClipboard(covertOcrResultToText(ocrResult));
                } else if (ocrAfterAction === OcrDetectAfterAction.CopyTextAndCloseWindow) {
                    writeTextToClipboard(covertOcrResultToText(ocrResult));
                    finishCapture?.();
                }
            }

            setDrawEvent({
                event: DrawEvent.OcrDetect,
                params: {
                    result: ocrResult,
                },
            });
            setDrawEvent(undefined);
        },
        [finishCapture, getAppSettings, getDrawState, setDrawEvent],
    );

    return (
        <>
            <OcrTool onReplace={onReplace} />

            <OcrResult
                zIndex={zIndexs.Draw_OcrResult}
                actionRef={ocrResultActionRef}
                onOcrDetect={onOcrDetect}
            />
        </>
    );
};
