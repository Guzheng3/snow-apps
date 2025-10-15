import { createContext } from 'react';

export const MenuLayoutContext = createContext<{
    noLayout: boolean;
    mainWindow: boolean;
    pathname: string;
}>({
    noLayout: false,
    mainWindow: false,
    pathname: '/',
});
