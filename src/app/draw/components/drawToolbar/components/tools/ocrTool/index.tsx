import { useCallback, useRef, useState } from 'react';
import { SubTools } from '../../subTools';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/types/draw';
import { DrawEvent, DrawEventParams, DrawEventPublisher } from '@/app/draw/extra';
import { Button } from 'antd';
import { OcrDetectResult } from '@/types/commands/ocr';
import { OcrTranslateIcon } from '@/components/icons';
import { useIntl } from 'react-intl';
import { ModalTranslator, ModalTranslatorActionType } from './components/modalTranslator';
import { useStateRef } from '@/hooks/useStateRef';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';

export const isOcrTool = (drawState: DrawState) => {
    return drawState === DrawState.OcrDetect || drawState === DrawState.OcrTranslate;
};

const OcrTool: React.FC<{
    onReplace: (result: OcrDetectResult, ignoreScale?: boolean) => void;
}> = ({ onReplace }) => {
    const intl = useIntl();

    const modalTranslatorActionRef = useRef<ModalTranslatorActionType>(undefined);

    const [enabled, setEnabled] = useState(false);
    const [ocrResult, setOcrResult, ocrResultRef] = useStateRef<OcrDetectResult | undefined>(
        undefined,
    );
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
                        title={intl.formatMessage({ id: 'draw.ocrDetect.translate' })}
                        type={'text'}
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
