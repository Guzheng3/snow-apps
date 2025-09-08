import { FormattedMessage } from 'react-intl';
import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { useCallback, useContext, useEffect, useMemo, useState } from 'react';
import { DrawState, DrawStatePublisher } from '../extra';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { Input, Radio } from 'antd';
import { ArrowIcon, DiamondIcon, LineIcon, RectIcon } from '@/components/icons';
import { DrawContext } from '@/app/fullScreenDraw/extra';
import { debounce } from 'es-toolkit';
import { useStateRef } from '@/hooks/useStateRef';
import { AppSettingsActionContext, AppSettingsGroup } from '@/app/contextWrap';

const WatermarkTextInput = () => {
    const { getDrawCoreAction } = useContext(DrawContext);
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const [watermarkText, setWatermarkText, watermarkTextRef] = useStateRef<string>('');

    const updateWatermarkText = useMemo(() => {
        return debounce(() => {
            if (!watermarkTextRef.current) {
                return;
            }

            const sceneElements = getDrawCoreAction()?.getExcalidrawAPI()?.getSceneElements();
            if (!sceneElements) {
                return;
            }

            const watermarkElement = sceneElements.find((element) => element.type === 'watermark');

            if (watermarkElement?.watermarkText === watermarkTextRef.current) {
                return;
            }

            getDrawCoreAction()
                ?.getExcalidrawAPI()
                ?.updateScene({
                    elements: sceneElements.map((element) => {
                        if (element.type === 'watermark') {
                            return { ...element, watermarkText: watermarkTextRef.current };
                        }
                        return element;
                    }),
                    captureUpdate: 'IMMEDIATELY',
                });

            updateAppSettings(
                AppSettingsGroup.Cache,
                {
                    lastWatermarkText: watermarkTextRef.current,
                },
                true,
                true,
                false,
                true,
                false,
            );
        }, 128);
    }, [getDrawCoreAction, updateAppSettings, watermarkTextRef]);

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
> = () => {
    const { setTool } = useContext(DrawContext);
    const [drawState, setDrawState] = useState<DrawState | undefined>(undefined);

    useStateSubscriber(
        DrawStatePublisher,
        useCallback((drawState: DrawState) => {
            setDrawState(drawState);
        }, []),
    );
    const drawStyleSubTools = useMemo(() => {
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
    }, [drawState, setTool]);

    const watermarkSubTools = useMemo(() => {
        if (drawState !== DrawState.Watermark) {
            return undefined;
        }

        return <WatermarkTextInput />;
    }, [drawState]);

    if (drawStyleSubTools) {
        return (
            <fieldset>
                <legend>
                    <FormattedMessage id="draw.drawStyleTool" />
                </legend>
                <div>{drawStyleSubTools}</div>
            </fieldset>
        );
    }

    if (watermarkSubTools) {
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
