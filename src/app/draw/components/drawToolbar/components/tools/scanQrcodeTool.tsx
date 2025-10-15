import { useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import { Spin, theme, Typography } from 'antd';
import { DrawContext } from '@/app/draw/types';
import { zIndexs } from '@/utils/zIndex';
import * as QrCodeScanner from 'qr-scanner-wechat';
import { AntdContext } from '@/contexts/antdContext';
import { useIntl } from 'react-intl';
import { useHotkeysApp } from '@/hooks/useHotkeysApp';
import { openUrl } from '@tauri-apps/plugin-opener';
import { getPlatformValue } from '@/utils/platform';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { DrawState } from '@/types/draw';
import { DrawStatePublisher } from '@/app/fullScreenDraw/components/drawCore/extra';
import { useMonitorRect } from '../../../statusBar';

const ScanQrcodeToolCore: React.FC<{}> = ({}) => {
    const intl = useIntl();

    const { token } = theme.useToken();
    const { message } = useContext(AntdContext);
    const { selectLayerActionRef, drawLayerActionRef, finishCapture } = useContext(DrawContext);

    const containerElementRef = useRef<HTMLDivElement>(null);
    const [containerStyle, setContainerStyle] = useState<React.CSSProperties>({});
    const [qrCode, setQrCode] = useState<string | undefined>(undefined);

    const {
        contentScale: [contentScale],
    } = useMonitorRect();

    const init = useCallback(async () => {
        const selectRect = selectLayerActionRef.current?.getSelectRect();
        if (!selectRect) {
            return;
        }

        setContainerStyle({
            width: (selectRect.max_x - selectRect.min_x) / window.devicePixelRatio,
            height: (selectRect.max_y - selectRect.min_y) / window.devicePixelRatio,
            left: selectRect.min_x / window.devicePixelRatio,
            top: selectRect.min_y / window.devicePixelRatio,
            opacity: 1,
        });

        const imageData = await drawLayerActionRef.current?.getImageData(selectRect);
        if (!imageData) {
            return;
        }

        const tempCanvas = document.createElement('canvas');
        tempCanvas.width = selectRect.max_x - selectRect.min_x;
        tempCanvas.height = selectRect.max_y - selectRect.min_y;
        const tempCtx = tempCanvas.getContext('2d');
        if (!tempCtx) {
            return;
        }

        tempCtx.putImageData(imageData, 0, 0);

        try {
            await QrCodeScanner.ready();
            const result = await QrCodeScanner.scan(tempCanvas);
            setQrCode(result.text ?? '');
            if (!result.text) {
                message.warning(
                    intl.formatMessage({
                        id: 'draw.extraTool.scanQrcode.error',
                    }),
                );
            }
        } catch (error) {
            console.error(error);
            message.warning(
                intl.formatMessage({
                    id: 'draw.extraTool.scanQrcode.error',
                }),
            );
        }
    }, [selectLayerActionRef, drawLayerActionRef, message, intl]);

    const inited = useRef(false);
    useEffect(() => {
        if (inited.current) {
            return;
        }

        inited.current = true;

        init();
    }, [init]);

    useHotkeysApp(
        getPlatformValue('Ctrl+A', 'Meta+A'),
        (event) => {
            event.preventDefault();

            const selection = window.getSelection();
            if (containerElementRef.current && selection) {
                const range = document.createRange();
                range.selectNodeContents(containerElementRef.current);
                selection.removeAllRanges();
                selection.addRange(range);
            }
        },
        {
            preventDefault: true,
            keyup: false,
            keydown: true,
        },
    );

    const qrCodeContent = useMemo(() => {
        if (!qrCode) {
            return '';
        }

        // 简单判断下
        if (qrCode.startsWith('http') || qrCode.startsWith('https')) {
            return (
                <a
                    onClick={() => {
                        openUrl(qrCode);
                        finishCapture();
                    }}
                >
                    {qrCode}
                </a>
            );
        }

        return qrCode;
    }, [finishCapture, qrCode]);

    return (
        <div
            style={{
                opacity: 0,
                ...containerStyle,
                width:
                    typeof containerStyle.width === 'number'
                        ? containerStyle.width / contentScale
                        : 0,
                height:
                    typeof containerStyle.height === 'number'
                        ? containerStyle.height / contentScale
                        : 0,
                background: token.colorBgContainer,
                padding: token.padding,
                position: 'fixed',
                zIndex: zIndexs.Draw_ScanQrcodeResult,
                pointerEvents: 'auto',
                boxSizing: 'border-box',
                transition: `opacity ${token.motionDurationFast} ${token.motionEaseInOut}`,
                transformOrigin: 'top left',
                transform: `scale(${contentScale})`,
            }}
            ref={containerElementRef}
        >
            {qrCode === undefined ? (
                <Spin spinning={true} />
            ) : (
                <Typography.Paragraph
                    copyable={
                        qrCode
                            ? {
                                  text: qrCode,
                                  onCopy: () => {
                                      finishCapture();
                                  },
                              }
                            : false
                    }
                >
                    {qrCodeContent}
                </Typography.Paragraph>
            )}
        </div>
    );
};

export const ScanQrcodeTool = () => {
    const [enable, setEnable] = useState(false);

    useStateSubscriber(
        DrawStatePublisher,
        useCallback(
            (drawState: DrawState) => {
                setEnable(drawState === DrawState.ScanQrcode);
            },
            [setEnable],
        ),
    );

    if (!enable) {
        return null;
    }

    return <ScanQrcodeToolCore />;
};
