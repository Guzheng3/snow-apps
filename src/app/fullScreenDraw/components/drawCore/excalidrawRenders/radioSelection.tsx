import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import {
    ExcalidrawEventCallbackPublisher,
    ExcalidrawEventCallbackParams,
    ExcalidrawEventCallbackType,
    DrawCoreContext,
} from '@/app/fullScreenDraw/components/drawCore/extra';
import { ExcalidrawPropsCustomOptions } from '@mg-chao/excalidraw/types';
import { Radio, Space } from 'antd';
import { useCallback, useContext, useEffect, useRef } from 'react';
import {
    convertSerialNumberTextElementIdToEllipseElementId,
    generateSerialNumber,
    isSerialNumberElement,
} from '../components/serialNumberTool';
import { last } from 'es-toolkit';

export const RadioSelection = ((props) => {
    const { getAction } = useContext(DrawCoreContext);

    const propsRef = useRef<typeof props | undefined>(undefined);

    const updateSerialNumber = useCallback(
        (value: number): boolean => {
            const selectedElementIds = getAction()?.getAppState()?.selectedElementIds;
            const elements = getAction()?.getExcalidrawAPI()?.getSceneElements();
            const selectedElements =
                elements && selectedElementIds
                    ? elements.filter((item) => selectedElementIds?.[item.id])
                    : [];

            if (!selectedElements || selectedElements.length === 0) {
                return false;
            }

            const appState = getAction()?.getAppState();
            if (!appState) {
                return false;
            }

            const selectedElementsMap = new Map(selectedElements.map((item) => [item.id, item]));
            const changedElementsMap = new Map();
            selectedElements.forEach((item) => {
                const changedElement = {
                    ...item,
                    fontSize: value as unknown as number,
                };
                if (!isSerialNumberElement(item)) {
                    return;
                }

                changedElementsMap.set(item.id, changedElement);

                if (item.type === 'text' && 'fontSize' in item) {
                    const ellipseElementId =
                        convertSerialNumberTextElementIdToEllipseElementId(item);
                    if (!ellipseElementId) {
                        return;
                    }

                    const ellipseElement = selectedElementsMap.get(ellipseElementId);
                    if (!ellipseElement) {
                        return;
                    }

                    const originPositionX = ellipseElement.x + ellipseElement.width / 2;
                    const originPositionY = ellipseElement.y + ellipseElement.height / 2;

                    const serialNumberTextElements = last(
                        generateSerialNumber(
                            {
                                x: originPositionX,
                                y: originPositionY,
                            },
                            1,
                            {
                                ...appState,
                                currentHoveredFontFamily: item.fontFamily,
                                currentItemFontSize: value,
                            },
                        ),
                    )!;

                    if (!('fontSize' in serialNumberTextElements)) {
                        return;
                    }
                    changedElement.x = serialNumberTextElements.x;
                    changedElement.y = serialNumberTextElements.y;
                    changedElement.width = serialNumberTextElements.width;
                    changedElement.height = serialNumberTextElements.height;
                    changedElement.fontSize = serialNumberTextElements.fontSize;
                } else if (item.type === 'ellipse') {
                    const originPositionX = item.x + item.width / 2;
                    const originPositionY = item.y + item.height / 2;

                    const serialNumberEllipseElement = generateSerialNumber(
                        {
                            x: originPositionX,
                            y: originPositionY,
                        },
                        1,
                        {
                            ...appState,
                            currentItemFontSize: value,
                        },
                    )[0];

                    if (!serialNumberEllipseElement) {
                        return;
                    }

                    changedElement.x = serialNumberEllipseElement.x;
                    changedElement.y = serialNumberEllipseElement.y;
                    changedElement.width = serialNumberEllipseElement.width;
                    changedElement.height = serialNumberEllipseElement.height;
                }
            });

            getAction()
                ?.getExcalidrawAPI()
                ?.updateScene({
                    elements: elements?.map((item) => changedElementsMap.get(item.id) ?? item),
                    appState: {
                        currentItemFontSize: value,
                    },
                    captureUpdate: 'IMMEDIATELY',
                });

            return true;
        },
        [getAction],
    );

    useEffect(() => {
        if (!('group' in props) || props.group !== 'font-size') {
            propsRef.current = props;
            return;
        }

        propsRef.current = {
            ...props,
            onChange: (value) => {
                if (!updateSerialNumber(value as unknown as number)) {
                    props.onChange?.(value);
                }
            },
        };
    }, [getAction, props, updateSerialNumber]);

    useStateSubscriber(
        ExcalidrawEventCallbackPublisher,
        useCallback((value: ExcalidrawEventCallbackParams | undefined) => {
            const currentProps = propsRef.current;
            if (!currentProps) {
                return;
            }

            if (!('group' in currentProps) || currentProps.group !== 'font-size') {
                return;
            }

            if (value?.event === ExcalidrawEventCallbackType.ChangeFontSize) {
                const fontSize = value.params.fontSize;
                const fontSizeIndex = currentProps.options.findIndex(
                    (option) => typeof option.value === 'number' && option.value === fontSize,
                );
                if (fontSizeIndex === -1) {
                    return;
                }
                const targetFontSize = currentProps.options[fontSizeIndex].value;
                if ('onChange' in currentProps) {
                    currentProps.onChange(targetFontSize);
                }
            }
        }, []),
    );

    if (props.type === 'button') {
        return (
            <Space>
                <Radio.Group value={props.value}>
                    {props.options.map((option) => (
                        <Radio.Button
                            key={option.text}
                            title={option.text}
                            value={option.value}
                            checked={option.active ?? props.value === option.value}
                        >
                            <div
                                className="radio-button-icon"
                                onClick={(event) =>
                                    props.onClick(
                                        option.value,
                                        event as unknown as React.MouseEvent<HTMLButtonElement>,
                                    )
                                }
                            >
                                {option.icon}
                            </div>
                        </Radio.Button>
                    ))}
                </Radio.Group>
            </Space>
        );
    }

    return (
        <Radio.Group value={props.value}>
            {props.options.map((option) => (
                <Radio.Button
                    name={props.group}
                    key={option.text}
                    title={option.text}
                    value={option.value}
                    checked={props.value === option.value}
                    data-testid={option.testId}
                >
                    <div
                        className="radio-button-icon"
                        onClick={() => {
                            if (!propsRef.current || !('onChange' in propsRef.current)) {
                                return;
                            }

                            propsRef.current.onChange(option.value);
                        }}
                    >
                        {option.icon}
                    </div>
                </Radio.Button>
            ))}
        </Radio.Group>
    );
}) as NonNullable<ExcalidrawPropsCustomOptions['pickerRenders']>['RadioSelection'];
