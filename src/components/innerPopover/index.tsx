import { Popover } from 'antd';
import { useCallback, useRef } from 'react';

/// 渲染在节点内部的 Popover
export const InnerPopover = ({ children, ...props }: React.ComponentProps<typeof Popover>) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const getPopupContainer = useCallback(() => {
        return containerRef.current ?? document.body;
    }, []);
    return (
        <div ref={containerRef}>
            <Popover {...props} getPopupContainer={getPopupContainer}>
                {children}
            </Popover>
        </div>
    );
};
