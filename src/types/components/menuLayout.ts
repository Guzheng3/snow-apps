'use client';

import React from 'react';
import { TabsProps } from 'antd';

export type RouteMapItem = {
    items: TabsProps['items'];
    hideTabs?: boolean;
};

export type RouteItem = {
    key: string;
    path: string | undefined;
    label: string;
    icon?: React.ReactNode;
    hideTabs?: boolean;
    children?: RouteItem[];
    tabs?: TabsProps['items'];
};
