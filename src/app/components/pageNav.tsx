import { RouteMapItem } from '@/types/components/menuLayout';
import { Tabs, TabsProps } from 'antd';
import { createStyles } from 'antd-style';
import { debounce } from 'es-toolkit';
import { useCallback, useEffect, useImperativeHandle, useMemo, useRef, useState } from 'react';

export type PageNavActionType = {
    updateActiveKey: (scrollTop: number) => void;
};

const useStyles = createStyles(({ token }, props: { hideTabs?: boolean }) => ({
    pageNav: {
        display: props.hideTabs ? 'none' : undefined,
        '& .ant-tabs': {
            marginTop: '-12px !important',
            padding: `0 ${token.padding}px !important`,
        },
        '& .ant-tabs-nav-wrap': {
            height: '32px !important',
        },
    },
}));

export const PageNav: React.FC<{
    tabItems: RouteMapItem;
    actionRef: React.RefObject<PageNavActionType | null>;
}> = ({ tabItems, actionRef }) => {
    const { styles } = useStyles({ hideTabs: tabItems.hideTabs });
    const [activeKey, setActiveKey] = useState<string | undefined>(tabItems.items?.[0]?.key);
    const tabItemsRef = useRef<TabsProps['items']>(tabItems.items);
    useEffect(() => {
        tabItemsRef.current = tabItems.items;
    }, [tabItems]);
    const anchorTopListRef = useRef<{ key: string; offsetTop: number }[]>([]);

    const updateActiveKey = useCallback(
        (scrollTop: number) => {
            const anchorTopList = anchorTopListRef.current;
            if (anchorTopList.length === 0) {
                return;
            }

            let targetKey = '';
            for (const anchor of anchorTopList) {
                if (anchor.offsetTop <= scrollTop) {
                    targetKey = anchor.key;
                } else {
                    break;
                }
            }

            if (!targetKey) {
                return;
            }

            setActiveKey(targetKey);
        },
        [anchorTopListRef],
    );
    const updateActiveKeyDebounce = useMemo(
        () => debounce(updateActiveKey, 256),
        [updateActiveKey],
    );
    useEffect(() => {
        if (!document) {
            return;
        }

        const tabs = tabItems.items;
        if (!tabs || tabs.length === 0) {
            return;
        }
        setActiveKey(tabs[0].key as string);

        anchorTopListRef.current = tabs.map((item) => {
            const element = document.getElementById(item.key as string);
            return {
                key: item.key as string,
                offsetTop: element
                    ? element.offsetTop - element.clientHeight
                    : Number.MAX_SAFE_INTEGER,
            };
        });

        updateActiveKeyDebounce(0);
    }, [tabItems, updateActiveKeyDebounce]);

    useImperativeHandle(
        actionRef,
        () => ({
            updateActiveKey: updateActiveKeyDebounce,
        }),
        [updateActiveKeyDebounce],
    );

    return (
        <div className={styles.pageNav}>
            <Tabs
                activeKey={activeKey}
                items={tabItems.items}
                size="small"
                onChange={(key) => {
                    const target = document.getElementById(key);
                    if (!target) {
                        return;
                    }
                    target.scrollIntoView({ behavior: 'smooth' });
                    setActiveKey(key);
                }}
            />
        </div>
    );
};
