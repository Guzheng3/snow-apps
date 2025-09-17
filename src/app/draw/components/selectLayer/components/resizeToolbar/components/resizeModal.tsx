import { CaptureBoundingBoxInfo } from '@/app/draw/extra';
import { ElementRect } from '@/commands';
import { useStateRef } from '@/hooks/useStateRef';
import ProForm, { ModalForm, ProFormDigit } from '@ant-design/pro-form';
import { Col, ColorPicker, Form, Row, theme } from 'antd';
import { useImperativeHandle, useState } from 'react';
import { FormattedMessage } from 'react-intl';

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

export const ResizeModal: React.FC<{
    actionRef: React.RefObject<ResizeModalActionType | undefined>;
    onFinish: (params: ResizeModalParams) => Promise<boolean>;
}> = ({ actionRef, onFinish }) => {
    const { token } = theme.useToken();
    const [open, setOpen, openRef] = useStateRef<boolean>(false);

    const [selectRectLimit, setSelectRectLimit] = useState<ElementRect>({
        min_x: 0,
        min_y: 0,
        max_x: 128,
        max_y: 128,
    });

    const [form] = Form.useForm();

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
                form.setFieldsValue({
                    minX: selectedRect.min_x,
                    minY: selectedRect.min_y,
                    width: selectedRect.max_x - selectedRect.min_x,
                    height: selectedRect.max_y - selectedRect.min_y,
                    radius,
                    shadowWidth: shadowConfig.shadowWidth,
                    shadowColor: shadowConfig.shadowColor,
                });
                setOpen(true);
            },
        };
    }, [form, openRef, setOpen]);

    return (
        <ModalForm
            form={form}
            open={open}
            onOpenChange={(value) => setOpen(value)}
            modalProps={{ centered: true }}
            width={500}
            title={<FormattedMessage id="draw.resizeModal" />}
            onDoubleClick={(e) => {
                e.stopPropagation();
                e.preventDefault();
            }}
            onClick={(e) => {
                e.stopPropagation();
                e.preventDefault();
            }}
            onFinish={onFinish}
        >
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
                        <ColorPicker showText placement="bottom" disabledAlpha />
                    </ProForm.Item>
                </Col>
            </Row>
        </ModalForm>
    );
};
