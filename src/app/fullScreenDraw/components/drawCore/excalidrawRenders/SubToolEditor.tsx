import { FormattedMessage } from 'react-intl';
import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { useCallback, useContext, useEffect, useMemo } from 'react';
import { DrawState, ExcalidrawEventPublisher } from '../extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { Input, Radio } from 'antd';
import { ArrowIcon, DiamondIcon, LineIcon, RectIcon } from '@/components/icons';
import { DrawContext } from '@/app/fullScreenDraw/extra';
import { debounce } from 'es-toolkit';
import { useStateRef } from '@/hooks/useStateRef';

const WatermarkTextInput = () => {
    const [, setDrawEvent] = useStateSubscriber(ExcalidrawEventPublisher, undefined);
    const { getDrawCoreAction } = useContext(DrawContext);
    const [watermarkText, setWatermarkText, watermarkTextRef] = useStateRef<string>('');

    const updateWatermarkText = useMemo(() => {
        return debounce(() => {
            setDrawEvent({
                event: 'onWatermarkTextChange',
                params: {
                    text: watermarkTextRef.current,
                },
            });
            setDrawEvent(undefined);
        }, 128);
    }, [setDrawEvent, watermarkTextRef]);

    const refreshWatermarkText = useMemo(() => {
        return debounce(() => {
            const sceneElements = getDrawCoreAction()?.getExcalidrawAPI()?.getSceneElements();
            if (!sceneElements) {
                return;
            }

            const watermarkElement = sceneElements.find((element) => element.type === 'watermark');

            setWatermarkText(watermarkElement?.watermarkText ?? '');
        }, 128);
    }, [getDrawCoreAction, setWatermarkText]);

    useEffect(() => {
        refreshWatermarkText();
    }, [refreshWatermarkText]);

    const onChange = useCallback(
        (e: React.ChangeEvent<HTMLInputElement>) => {
            setWatermarkText(e.target.value);
            updateWatermarkText();
        },
        [setWatermarkText, updateWatermarkText],
    );

    return <Input value={watermarkText} onChange={onChange} />;
};

const SubToolEditor: NonNullable<
    NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['SubToolEditor']
> = ({ appState, targetElements }) => {
    const { setTool } = useContext(DrawContext);

    const drawStyleSubTools = useMemo(() => {
        let drawState: DrawState | undefined = undefined;
        switch (appState.activeTool.type) {
            case 'rectangle':
                drawState = DrawState.Rect;
                break;
            case 'diamond':
                drawState = DrawState.Diamond;
                break;
            case 'arrow':
                drawState = DrawState.Arrow;
                break;
            case 'line':
                drawState = DrawState.Line;
                break;
        }

        if (drawState === DrawState.Rect || drawState === DrawState.Diamond) {
            return (
                <Radio.Group value={drawState}>
                    <Radio.Button value={DrawState.Rect}>
                        <div
                            className="subtool-radio-button-icon"
                            onClick={() => {
                                setTool(DrawState.Rect);
                            }}
                        >
                            <RectIcon style={{ fontSize: '1em' }} />
                        </div>
                    </Radio.Button>
                    <Radio.Button value={DrawState.Diamond}>
                        <div
                            className="subtool-radio-button-icon"
                            onClick={() => {
                                setTool(DrawState.Diamond);
                            }}
                        >
                            <DiamondIcon style={{ fontSize: '1em' }} />
                        </div>
                    </Radio.Button>
                </Radio.Group>
            );
        }

        if (drawState === DrawState.Arrow || drawState === DrawState.Line) {
            return (
                <Radio.Group value={drawState} style={{ display: 'flex' }}>
                    <Radio.Button value={DrawState.Arrow}>
                        <div
                            className="subtool-radio-button-icon"
                            onClick={() => {
                                setTool(DrawState.Arrow);
                            }}
                        >
                            <ArrowIcon style={{ fontSize: '0.83em' }} />
                        </div>
                    </Radio.Button>
                    <Radio.Button value={DrawState.Line}>
                        <div
                            className="subtool-radio-button-icon"
                            onClick={() => {
                                setTool(DrawState.Line);
                            }}
                        >
                            <LineIcon style={{ fontSize: '1.16em' }} />
                        </div>
                    </Radio.Button>
                </Radio.Group>
            );
        }

        return undefined;
    }, [appState.activeTool.type, setTool]);

    const watermarkSubTools = useMemo(() => {
        if (appState.activeTool.type !== 'watermark') {
            return undefined;
        }

        return <WatermarkTextInput />;
    }, [appState.activeTool.type]);

    if (drawStyleSubTools && targetElements.length === 0) {
        return (
            <fieldset>
                <legend>
                    <FormattedMessage id="draw.drawStyleTool" />
                </legend>
                <div>{drawStyleSubTools}</div>
            </fieldset>
        );
    }

    if (watermarkSubTools && targetElements.length === 0) {
        return (
            <fieldset>
                <legend>
                    <FormattedMessage id="draw.watermarkTool.text" />
                </legend>
                <div>{watermarkSubTools}</div>
            </fieldset>
        );
    }

    return <></>;
};

export default SubToolEditor;
