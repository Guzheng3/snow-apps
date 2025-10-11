'use client';

import { AppSettingsData, AppSettingsPublisher } from '@/app/contextWrap';
import { useAppSettingsLoad } from '@/hooks/useAppSettingsLoad';
import { CaptureHistoryItem } from '@/utils/appStore';
import { CaptureHistory, getCaptureHistoryImageAbsPath } from '@/utils/captureHistory';
import { ReloadOutlined } from '@ant-design/icons';
import { ActionType, ProList } from '@ant-design/pro-components';
import { Button, Tag } from 'antd';
import dayjs from 'dayjs';
import React, { useCallback, useContext, useEffect, useRef, useState } from 'react';
import { FormattedMessage } from 'react-intl';
import { convertFileSrc } from '@tauri-apps/api/core';
import { useStateRef } from '@/hooks/useStateRef';
import { executeScreenshot, ScreenshotType } from '@/functions/screenshot';
import { EventListenerContext } from '@/components/eventListener';
import { useStateSubscriber } from '@/hooks/useStateSubscriber';
import { CaptureHistoryRecordItem } from './extra';
import { CaptureHistoryItemActions } from './components/captureHistoryItemActions';
import { CaptureHistoryItemPreview } from './components/captureHistoryItemPreview';

const CaptureHistoryPage = () => {
    const [loading, setLoading] = useState(true);
    const [dataSource, setDataSource, dataSourceRef] = useStateRef<
        CaptureHistoryItem[] | undefined
    >(undefined);
    const captureHistoryRef = useRef<CaptureHistory | undefined>(undefined);

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

            captureHistoryRef.current = new CaptureHistory();
            await captureHistoryRef.current.init();
            const list = (await captureHistoryRef.current.getList(appSettings)).sort(
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

                    const filterData = dataSourceRef.current.filter((item) => {
                        if (startTs && endTs) {
                            return item.create_ts >= startTs && item.create_ts <= endTs;
                        }
                        return true;
                    });
                    const data = await Promise.all(
                        filterData.slice(startIndex, startIndex + pageSize).map(async (item) => {
                            const file_path = await getCaptureHistoryImageAbsPath(item.file_name);
                            const capture_result_file_path = item.capture_result_file_name
                                ? await getCaptureHistoryImageAbsPath(item.capture_result_file_name)
                                : undefined;
                            return {
                                ...item,
                                file_path,
                                file_url: convertFileSrc(file_path),
                                capture_result_file_path,
                                capture_result_file_url: capture_result_file_path
                                    ? convertFileSrc(capture_result_file_path)
                                    : undefined,
                            };
                        }),
                    );

                    setLoading(false);

                    return {
                        data,
                        success: true,
                        pageSize: pageSize,
                        total: data.length,
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
                        cardActionProps: 'extra',
                        render: (_, item: CaptureHistoryRecordItem) => {
                            return (
                                <CaptureHistoryItemActions
                                    item={item}
                                    reloadList={reloadList}
                                    captureHistoryRef={captureHistoryRef}
                                />
                            );
                        },
                    },
                    extra: {
                        search: false,
                        render: (_: unknown, item: CaptureHistoryRecordItem) => {
                            return <CaptureHistoryItemPreview item={item} />;
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
