'use client';

import { AppSettingsData, AppSettingsPublisher } from '@/app/contextWrap';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { CaptureHistoryItem } from '@/utils/appStore';
import { CaptureHistory, getCaptureHistoryImageAbsPath } from '@/utils/captureHistory';
import { CopyOutlined, EditOutlined, ReloadOutlined } from '@ant-design/icons';
import { ActionType, ProList } from '@ant-design/pro-components';
import { Button, Tag } from 'antd';
import dayjs from 'dayjs';
import React, { useCallback, useContext, useEffect, useRef, useState } from 'react';
import { FormattedMessage } from 'react-intl';
import { Image } from 'antd';
import { convertFileSrc } from '@tauri-apps/api/core';
import { useStateRef } from '@/hooks/useStateRef';
import { writeFilePathToClipboard } from '@/utils/clipboard';
import { executeScreenshot, ScreenshotType } from '@/functions/screenshot';
import { EventListenerContext } from '@/components/eventListener';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';

type CaptureHistoryRecordItem = CaptureHistoryItem & { file_path: string; file_url: string };

const CaptureHistoryPage = () => {
    const [loading, setLoading] = useState(true);
    const [dataSource, setDataSource, dataSourceRef] = useStateRef<
        CaptureHistoryItem[] | undefined
    >(undefined);

    const actionRef = useRef<ActionType>(null);

    const initedRef = useRef(false);
    const [getAppSettings] = useStateSubscriber(AppSettingsPublisher, undefined);
    const initDataSource = useCallback(
        async (appSettings: AppSettingsData) => {
            if (initedRef.current) {
                return;
            }
            initedRef.current = true;

            setLoading(true);

            const captureHistory = new CaptureHistory();
            await captureHistory.init();
            const list = (await captureHistory.getList(appSettings)).sort(
                (a, b) => b.create_ts - a.create_ts,
            );
            setDataSource(list);

            actionRef.current?.reload();
        },
        [setDataSource],
    );
    const reloadList = useCallback(async () => {
        initedRef.current = false;
        initDataSource(getAppSettings());
    }, [getAppSettings, initDataSource]);

    useAppSettingsLoad(initDataSource, false);

    const { addListener, removeListener } = useContext(EventListenerContext);
    useEffect(() => {
        const listenerId = addListener('on-capture-history-change', () => {
            reloadList();
        });
        return () => {
            removeListener(listenerId);
        };
    }, [addListener, reloadList, removeListener]);

    return (
        <>
            <ProList<CaptureHistoryRecordItem>
                itemLayout="vertical"
                rowKey="id"
                headerTitle={
                    <>
                        <FormattedMessage id="tools.captureHistory.count" />
                        {`: ${dataSource?.length ?? '-'}`}
                    </>
                }
                loading={loading}
                className="capture-history-list"
                actionRef={actionRef}
                toolBarRender={() => [
                    <Button
                        key="reload"
                        type="text"
                        loading={loading}
                        onClick={() => {
                            reloadList();
                        }}
                        icon={<ReloadOutlined />}
                    />,
                ]}
                request={async (params) => {
                    setLoading(true);

                    if (!dataSourceRef.current) {
                        return {
                            data: [],
                            success: false,
                            pageSize: 10,
                            total: 0,
                        };
                    }

                    const pageSize = params.pageSize ?? 10;
                    const current = params.current ?? 1;
                    const startIndex = (current - 1) * pageSize;

                    let startTs: number | undefined = undefined;
                    let endTs: number | undefined = undefined;
                    if (
                        'create_ts' in params &&
                        Array.isArray(params.create_ts) &&
                        params.create_ts.length === 2
                    ) {
                        startTs = dayjs(params.create_ts[0], 'YYYY-MM-DD HH:mm:ss').valueOf();
                        endTs = dayjs(params.create_ts[1], 'YYYY-MM-DD HH:mm:ss').valueOf();
                    }

                    const data = await Promise.all(
                        dataSourceRef.current
                            .filter((item) => {
                                if (startTs && endTs) {
                                    return item.create_ts >= startTs && item.create_ts <= endTs;
                                }
                                return true;
                            })
                            .slice(startIndex, startIndex + pageSize)
                            .map(async (item) => {
                                const file_path = await getCaptureHistoryImageAbsPath(
                                    item.file_name,
                                );
                                return {
                                    ...item,
                                    file_path,
                                    file_url: convertFileSrc(file_path),
                                };
                            }),
                    );

                    setLoading(false);

                    return {
                        data,
                        success: true,
                        pageSize: 10,
                        total: dataSourceRef.current.length,
                    };
                }}
                pagination={{
                    defaultPageSize: 20,
                    showSizeChanger: true,
                }}
                search={{
                    filterType: 'light',
                }}
                metas={{
                    title: {
                        title: <FormattedMessage id="tools.captureHistory.date" />,
                        render: (_, item) => {
                            return (
                                <div
                                    onClick={() => {
                                        executeScreenshot(
                                            ScreenshotType.SwitchCaptureHistory,
                                            undefined,
                                            item.id,
                                        );
                                    }}
                                >
                                    <FormattedMessage id="tools.captureHistory.date" />
                                    {`: ${dayjs(item.create_ts).format('YYYY-MM-DD HH:mm:ss')}`}
                                </div>
                            );
                        },
                        search: true,
                        dataIndex: 'create_ts',
                        valueType: 'dateTimeRange',
                    },
                    description: {
                        search: false,
                        render: (_, item) => {
                            const { selected_rect } = item;

                            return (
                                <>
                                    <Tag>
                                        <FormattedMessage id="tools.captureHistory.position" />
                                        {`: ${selected_rect.min_x} , ${selected_rect.min_y}`}
                                    </Tag>
                                    <Tag>
                                        <FormattedMessage id="tools.captureHistory.size" />
                                        {`: ${selected_rect.max_x - selected_rect.min_x} x ${selected_rect.max_y - selected_rect.min_y}`}
                                    </Tag>
                                    <Tag>
                                        <FormattedMessage id="tools.captureHistory.drawElements" />
                                        {`: ${item.excalidraw_elements?.length ?? 0}`}
                                    </Tag>
                                </>
                            );
                        },
                    },
                    actions: {
                        search: false,
                        render: (_, item: CaptureHistoryRecordItem) => [
                            <Button
                                key="view"
                                onClick={() => {
                                    executeScreenshot(
                                        ScreenshotType.SwitchCaptureHistory,
                                        undefined,
                                        item.id,
                                    );
                                }}
                                size="small"
                                type="link"
                                icon={<EditOutlined />}
                            >
                                <FormattedMessage id="tools.captureHistory.switch" />
                            </Button>,
                            <Button
                                onClick={() => {
                                    writeFilePathToClipboard(item.file_path);
                                }}
                                key="copy"
                                size="small"
                                type="link"
                                icon={<CopyOutlined />}
                            >
                                <FormattedMessage id="tools.captureHistory.copy" />
                            </Button>,
                        ],
                    },
                    extra: {
                        search: false,
                        render: (_: unknown, item: CaptureHistoryRecordItem) => {
                            return (
                                <Image
                                    alt="preview"
                                    loading="lazy"
                                    key={item.id}
                                    src={item.file_url}
                                    width={350}
                                    height={128}
                                    style={{ objectFit: 'contain' }}
                                />
                            );
                        },
                    },
                }}
            />

            <style jsx>{`
                :global(.capture-history-list .ant-pro-card-body) {
                    padding-inline: 0 !important;
                    padding-block: 0 !important;
                }
            `}</style>
        </>
    );
};

export default CaptureHistoryPage;
