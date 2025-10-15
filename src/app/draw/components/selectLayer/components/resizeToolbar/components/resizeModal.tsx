import {
    AppSettingsActionContext,
    AppSettingsData,
    AppSettingsGroup,
    AppSettingsPublisher,
    defaultAppSettingsData,
    SelectRectPreset,
} from '@/app/contextWrap';
import { CaptureBoundingBoxInfo } from '@/app/draw/extra';
import { ElementRect } from '@/commands';
import { HotkeysScope } from '@/components/globalLayoutExtra';
import { useStateRef } from '@/hooks/useStateRef';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { PlusOutlined } from '@ant-design/icons';
import ProForm, {
    ModalForm,
    ProFormDigit,
    ProFormList,
    ProFormSelect,
    ProFormSwitch,
    ProFormText,
} from '@ant-design/pro-form';
import { Col, ColorPicker, Flex, Form, Row, Space, theme } from 'antd';
import { AggregationColor } from 'antd/es/color-picker/color';
import {
    useCallback,
    useContext,
    useEffect,
    useImperativeHandle,
    useMemo,
    useRef,
    useState,
} from 'react';
import { useHotkeysContext } from 'react-hotkeys-hook';
import { FormattedMessage, useIntl } from 'react-intl';
import RSC from 'react-scrollbars-custom';

export type ResizeModalActionType = {
    show: (
        selectedRect: ElementRect,
        radius: number,
        shadowConfig: { shadowWidth: number; shadowColor: string },
        captureBoundingBoxInfo: CaptureBoundingBoxInfo,
    ) => void;
};

export type ResizeModalParams = {
    minX: number;
    minY: number;
    width: number;
    height: number;
    radius: number;
    shadowWidth: number;
    shadowColor: unknown;
};

export type QuickSetType = 'previousSelectRect' | 'currentSelectRect' | number | 'addPreset';

export const ResizeModal: React.FC<{
    actionRef: React.RefObject<ResizeModalActionType | undefined>;
    onFinish: (params: ResizeModalParams) => Promise<boolean>;
}> = ({ actionRef, onFinish }) => {
    const { token } = theme.useToken();
    const intl = useIntl();
    const [open, setOpen, openRef] = useStateRef<boolean>(false);

    const [selectRectPresetList, setSelectRectPresetList, selectRectPresetListRef] = useStateRef<
        SelectRectPreset[]
    >([]);
    const { updateAppSettings } = useContext(AppSettingsActionContext);
    const [getAppSettings] = useStateSubscriber(
        AppSettingsPublisher,
        useCallback(
            (appSettings: AppSettingsData) => {
                setSelectRectPresetList(
                    appSettings[AppSettingsGroup.FunctionScreenshot].selectRectPresetList,
                );
            },
            [setSelectRectPresetList],
        ),
    );

    const [selectRectLimit, setSelectRectLimit] = useState<ElementRect>({
        min_x: 0,
        min_y: 0,
        max_x: 83,
        max_y: 83,
    });

    const [form] = Form.useForm();

    const [currentSelectRect, setCurrentSelectRect, currentSelectRectRef] = useStateRef<
        ElementRect | undefined
    >(undefined);

    const aspectRatioRef = useRef<number>(undefined);
    useImperativeHandle(actionRef, () => {
        return {
            show: (
                selectedRect: ElementRect,
                radius: number,
                shadowConfig: { shadowWidth: number; shadowColor: string },
                captureBoundingBoxInfo: CaptureBoundingBoxInfo,
            ) => {
                if (openRef.current) {
                    return;
                }

                setSelectRectLimit(
                    captureBoundingBoxInfo.transformMonitorRect(captureBoundingBoxInfo.rect),
                );
                setCurrentSelectRect(selectedRect);
                form.setFieldsValue({
                    minX: selectedRect.min_x,
                    minY: selectedRect.min_y,
                    width: selectedRect.max_x - selectedRect.min_x,
                    height: selectedRect.max_y - selectedRect.min_y,
                    radius,
                    shadowWidth: shadowConfig.shadowWidth,
                    shadowColor: shadowConfig.shadowColor,
                    lockAspectRatio: undefined,
                });
                setOpen(true);
            },
        };
    }, [form, openRef, setCurrentSelectRect, setOpen]);

    const handleDoubleClick = useCallback((e: React.MouseEvent<HTMLFormElement>) => {
        e.stopPropagation();
        e.preventDefault();
    }, []);
    const handleClick = useCallback((e: React.MouseEvent<HTMLFormElement>) => {
        e.stopPropagation();
        e.preventDefault();
    }, []);

    const [quickSet, setQuickSet] = useState<QuickSetType | null>(null);
    const quickSetOptions = useMemo(() => {
        return [
            {
                label: <FormattedMessage id="draw.selectRectParams.quickSet.selectRectSet" />,
                options: [
                    {
                        label: (
                            <FormattedMessage id="draw.selectRectParams.quickSet.currentSelectRect" />
                        ),
                        value: 'currentSelectRect',
                    },
                    {
                        label: (
                            <FormattedMessage id="draw.selectRectParams.quickSet.previousSelectRect" />
                        ),
                        value: 'previousSelectRect',
                    },
                ],
            },
            {
                label: <FormattedMessage id="draw.selectRectParams.quickSet.preset" />,
                options: [
                    ...selectRectPresetList.map((preset, index) => ({
                        label: preset.name,
                        value: index,
                    })),
                    {
                        label: (
                            <Space style={{ color: token.colorPrimary }}>
                                <PlusOutlined />
                                <FormattedMessage id="draw.selectRectParams.quickSet.addPreset" />
                            </Space>
                        ),
                        value: 'addPreset',
                    },
                ],
            },
        ];
    }, [selectRectPresetList, token.colorPrimary]);

    const onQuickSetChange = useCallback(
        (value: QuickSetType) => {
            if (value === 'currentSelectRect' || value === 'previousSelectRect') {
                let targetSelectRect: ElementRect | undefined;
                if (value === 'previousSelectRect') {
                    targetSelectRect = getAppSettings()[AppSettingsGroup.Cache].prevSelectRect;
                } else {
                    targetSelectRect = currentSelectRectRef.current ?? {
                        min_x: 0,
                        min_y: 0,
                        max_x: 0,
                        max_y: 0,
                    };
                }

                form.setFieldsValue({
                    minX: targetSelectRect.min_x,
                    minY: targetSelectRect.min_y,
                    width: targetSelectRect.max_x - targetSelectRect.min_x,
                    height: targetSelectRect.max_y - targetSelectRect.min_y,
                    lockAspectRatio: false,
                });
                setQuickSet(value);
                return;
            }

            if (value === 'addPreset') {
                form.setFieldsValue({
                    selectRectPresetList:
                        selectRectPresetListRef.current.length === 0
                            ? [
                                  {
                                      name: '',
                                      selectParams: {
                                          minX: currentSelectRect?.min_x ?? 0,
                                          minY: currentSelectRect?.min_y ?? 0,
                                          width:
                                              (currentSelectRect?.max_x ?? 0) -
                                              (currentSelectRect?.min_x ?? 0),
                                          height:
                                              (currentSelectRect?.max_y ?? 0) -
                                              (currentSelectRect?.min_y ?? 0),
                                          radius: defaultAppSettingsData[AppSettingsGroup.Cache]
                                              .selectRectRadius,
                                          shadowWidth:
                                              defaultAppSettingsData[AppSettingsGroup.Cache]
                                                  .selectRectShadowWidth,
                                          shadowColor:
                                              defaultAppSettingsData[AppSettingsGroup.Cache]
                                                  .selectRectShadowColor,
                                          lockAspectRatio: false,
                                      },
                                  },
                              ]
                            : [
                                  ...selectRectPresetListRef.current.map((item) => ({
                                      ...item,
                                      selectParams: {
                                          ...item.selectParams,
                                          shadowColor:
                                              typeof item.selectParams.shadowColor === 'object'
                                                  ? defaultAppSettingsData[AppSettingsGroup.Cache]
                                                        .selectRectShadowColor
                                                  : item.selectParams.shadowColor,
                                      },
                                  })),
                              ],
                });
                setQuickSet(value);
                return;
            }

            if (typeof value === 'number' && selectRectPresetListRef.current[value]) {
                const targetParams = selectRectPresetListRef.current[value].selectParams;
                form.setFieldsValue({
                    minX: targetParams.minX,
                    minY: targetParams.minY,
                    width: targetParams.width,
                    height: targetParams.height,
                    radius: targetParams.radius,
                    shadowWidth: targetParams.shadowWidth,
                    shadowColor:
                        typeof targetParams.shadowColor === 'object'
                            ? defaultAppSettingsData[AppSettingsGroup.Cache].selectRectShadowColor
                            : targetParams.shadowColor,
                    lockAspectRatio: targetParams.lockAspectRatio,
                });
                if (targetParams.lockAspectRatio) {
                    aspectRatioRef.current = targetParams.height / targetParams.width;
                } else {
                    aspectRatioRef.current = undefined;
                }
                setQuickSet(null);
                return;
            }

            setQuickSet(null);
            return;
        },
        [
            currentSelectRect?.max_x,
            currentSelectRect?.max_y,
            currentSelectRect?.min_x,
            currentSelectRect?.min_y,
            currentSelectRectRef,
            form,
            getAppSettings,
            selectRectPresetListRef,
        ],
    );

    const containerRef = useRef<HTMLDivElement>(null);

    const enableAddPreset = quickSet === 'addPreset';

    const getPopupContainer = useCallback(() => {
        return containerRef.current ?? document.body;
    }, []);

    const { disableScope, enableScope } = useHotkeysContext();
    useEffect(() => {
        if (open) {
            disableScope(HotkeysScope.DrawTool);
        } else {
            enableScope(HotkeysScope.DrawTool);
        }

        return () => {
            enableScope(HotkeysScope.DrawTool);
        };
    }, [open, disableScope, enableScope]);

    const lockAspectRatioValue = Form.useWatch('lockAspectRatio', form);
    useEffect(() => {
        if (lockAspectRatioValue) {
            if (!aspectRatioRef.current) {
                aspectRatioRef.current = form.getFieldValue('height') / form.getFieldValue('width');
            }
        } else {
            aspectRatioRef.current = undefined;
        }
    }, [form, lockAspectRatioValue]);

    const widthValue = Form.useWatch('width', form);
    useEffect(() => {
        if (aspectRatioRef.current) {
            form.setFieldsValue({ height: widthValue * aspectRatioRef.current });
        }
    }, [form, aspectRatioRef, widthValue]);
    const heightValue = Form.useWatch('height', form);
    useEffect(() => {
        if (aspectRatioRef.current) {
            form.setFieldsValue({ width: heightValue / aspectRatioRef.current });
        }
    }, [form, aspectRatioRef, heightValue]);

    return (
        <ModalForm
            form={form}
            open={open}
            onOpenChange={(value) => {
                if (!value && enableAddPreset) {
                    setQuickSet(null);
                    return;
                }

                setOpen(value);
            }}
            modalProps={{ centered: true }}
            width={500}
            title={<FormattedMessage id="draw.resizeModal" />}
            onDoubleClick={handleDoubleClick}
            onClick={handleClick}
            onFinish={async (...params) => {
                if (enableAddPreset) {
                    updateAppSettings(
                        AppSettingsGroup.FunctionScreenshot,
                        {
                            selectRectPresetList: params[0].selectRectPresetList.map(
                                (item: { selectParams: { shadowColor: unknown } }) => {
                                    console.log('item', item);
                                    if (typeof item.selectParams.shadowColor === 'object') {
                                        item.selectParams.shadowColor = (
                                            item.selectParams.shadowColor as AggregationColor
                                        ).toHexString();
                                    }

                                    return {
                                        ...item,
                                    };
                                },
                            ),
                        },
                        true,
                        true,
                        false,
                        true,
                        false,
                    );
                    setQuickSet(null);
                    return;
                }

                return await onFinish(...params);
            }}
        >
            <div ref={containerRef}>
                {!enableAddPreset && (
                    <>
                        <Row>
                            <Col span={12}>
                                <ProFormSelect<QuickSetType>
                                    label={<FormattedMessage id="draw.selectRectParams.quickSet" />}
                                    options={quickSetOptions}
                                    style={{ minWidth: 200 }}
                                    fieldProps={{
                                        value: quickSet,
                                        getPopupContainer,
                                    }}
                                    onChange={onQuickSetChange}
                                />
                            </Col>
                        </Row>
                        <Row gutter={token.marginLG}>
                            <Col span={12}>
                                <ProFormDigit
                                    name="minX"
                                    label={<FormattedMessage id="draw.positionX" />}
                                    min={selectRectLimit.min_x}
                                    max={selectRectLimit.max_x - 1}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>
                            <Col span={12}>
                                <ProFormDigit
                                    name="minY"
                                    label={<FormattedMessage id="draw.positionY" />}
                                    min={selectRectLimit.min_y}
                                    max={selectRectLimit.max_y - 1}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>
                        </Row>
                        <Row gutter={token.marginLG}>
                            <Col span={12}>
                                <ProFormDigit
                                    name="width"
                                    label={<FormattedMessage id="draw.width" />}
                                    min={1}
                                    max={selectRectLimit.max_x - selectRectLimit.min_x}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>
                            <Col span={12}>
                                <ProFormDigit
                                    name="height"
                                    label={<FormattedMessage id="draw.height" />}
                                    min={1}
                                    max={selectRectLimit.max_y - selectRectLimit.min_y}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>
                        </Row>
                        <Row gutter={token.marginLG}>
                            <Col span={12}>
                                <ProFormSwitch
                                    name="lockAspectRatio"
                                    label={<FormattedMessage id="draw.lockAspectRatio" />}
                                />
                            </Col>
                        </Row>
                        <Row gutter={token.marginLG}>
                            <Col span={12}>
                                <ProFormDigit
                                    name="radius"
                                    label={<FormattedMessage id="draw.radius" />}
                                    min={0}
                                    max={256}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>
                        </Row>

                        <Row gutter={token.marginLG}>
                            <Col span={12}>
                                <ProFormDigit
                                    name="shadowWidth"
                                    label={<FormattedMessage id="draw.shadowWidth" />}
                                    min={0}
                                    max={32}
                                    fieldProps={{ precision: 0 }}
                                />
                            </Col>

                            <Col span={12}>
                                <ProForm.Item
                                    name="shadowColor"
                                    label={<FormattedMessage id="draw.shadowColor" />}
                                    required={false}
                                >
                                    <ColorPicker
                                        getPopupContainer={getPopupContainer}
                                        showText
                                        placement="bottom"
                                        disabledAlpha
                                    />
                                </ProForm.Item>
                            </Col>
                        </Row>
                    </>
                )}

                {enableAddPreset && (
                    <RSC style={{ minHeight: '500px' }}>
                        <ProFormList
                            name="selectRectPresetList"
                            label={<FormattedMessage id="draw.selectRectParams.quickSet.preset" />}
                            creatorButtonProps={{
                                creatorButtonText: (
                                    <FormattedMessage id="draw.selectRectParams.quickSet.addPreset" />
                                ),
                            }}
                            className="select-rect-preset-list"
                            min={0}
                            itemRender={({ listDom, action }) => (
                                <Flex align="end" justify="space-between">
                                    {listDom}
                                    <div>{action}</div>
                                </Flex>
                            )}
                            creatorRecord={() => ({
                                name: '',
                                selectParams: {
                                    minX: currentSelectRect?.min_x ?? 0,
                                    minY: currentSelectRect?.min_y ?? 0,
                                    width:
                                        (currentSelectRect?.max_x ?? 0) -
                                        (currentSelectRect?.min_x ?? 0),
                                    height:
                                        (currentSelectRect?.max_y ?? 0) -
                                        (currentSelectRect?.min_y ?? 0),
                                    radius: defaultAppSettingsData[AppSettingsGroup.Cache]
                                        .selectRectRadius,
                                    shadowWidth:
                                        defaultAppSettingsData[AppSettingsGroup.Cache]
                                            .selectRectShadowWidth,
                                    shadowColor:
                                        defaultAppSettingsData[AppSettingsGroup.Cache]
                                            .selectRectShadowColor,
                                    lockAspectRatio: false,
                                },
                            })}
                        >
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormText
                                        name={['name']}
                                        label={intl.formatMessage({
                                            id: 'draw.selectRectParams.quickSet.presetName',
                                        })}
                                        required
                                        rules={[{ required: true, type: 'string' }]}
                                        fieldProps={{
                                            onKeyDown: (e) => {
                                                e.stopPropagation();
                                            },
                                            onKeyUp: (e) => {
                                                e.stopPropagation();
                                            },
                                        }}
                                    />
                                </Col>
                            </Row>
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'minX']}
                                        label={<FormattedMessage id="draw.positionX" />}
                                        min={selectRectLimit.min_x}
                                        max={selectRectLimit.max_x - 1}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'minY']}
                                        label={<FormattedMessage id="draw.positionY" />}
                                        min={selectRectLimit.min_y}
                                        max={selectRectLimit.max_y - 1}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>
                            </Row>
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'width']}
                                        label={<FormattedMessage id="draw.width" />}
                                        min={1}
                                        max={selectRectLimit.max_x - selectRectLimit.min_x}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'height']}
                                        label={<FormattedMessage id="draw.height" />}
                                        min={1}
                                        max={selectRectLimit.max_y - selectRectLimit.min_y}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>
                            </Row>
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormSwitch
                                        name={['selectParams', 'lockAspectRatio']}
                                        label={<FormattedMessage id="draw.lockAspectRatio" />}
                                    />
                                </Col>
                            </Row>
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'radius']}
                                        label={<FormattedMessage id="draw.radius" />}
                                        min={0}
                                        max={256}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>
                            </Row>
                            <Row gutter={token.marginLG}>
                                <Col span={12}>
                                    <ProFormDigit
                                        name={['selectParams', 'shadowWidth']}
                                        label={<FormattedMessage id="draw.shadowWidth" />}
                                        min={0}
                                        max={32}
                                        fieldProps={{ precision: 0 }}
                                    />
                                </Col>

                                <Col span={12}>
                                    <ProForm.Item
                                        name={['selectParams', 'shadowColor']}
                                        label={<FormattedMessage id="draw.shadowColor" />}
                                        required={false}
                                    >
                                        <ColorPicker
                                            getPopupContainer={getPopupContainer}
                                            showText
                                            placement="bottom"
                                            disabledAlpha
                                        />
                                    </ProForm.Item>
                                </Col>
                            </Row>
                        </ProFormList>
                    </RSC>
                )}
            </div>
        </ModalForm>
    );
};
