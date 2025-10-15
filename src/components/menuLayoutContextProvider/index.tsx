import { MenuLayoutContext } from '@/contexts/menuLayoutContext';
import { usePathname } from 'next/navigation';
import { useMemo } from 'react';

export const MenuLayoutContextProvider = ({ children }: { children: React.ReactNode }) => {
    const pathname = usePathname();
    const noLayout = useMemo(
        () =>
            pathname === '/draw' ||
            pathname === '/fixedContent' ||
            pathname === '/fullScreenDraw' ||
            pathname === '/fullScreenDraw/switchMouseThrough' ||
            pathname === '/videoRecord' ||
            pathname === '/videoRecord/toolbar' ||
            pathname === '/idle',
        [pathname],
    );
    const mainWindow = !noLayout;
    return (
        <MenuLayoutContext.Provider value={{ noLayout, pathname, mainWindow }}>
            {children}
        </MenuLayoutContext.Provider>
    );
};
