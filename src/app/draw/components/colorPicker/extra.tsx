import { useCallback, useMemo, useRef } from 'react';
import { debounce } from 'es-toolkit';

export const useMoveCursor = (): {
    disableMouseMove: () => void;
    isDisableMouseMove: () => boolean;
    enableMouseMove: () => void;
} => {
    const moveCursorFinishedRef = useRef(true);
    const disableMouseMove = useCallback(() => {
        moveCursorFinishedRef.current = false;
    }, []);
    const isDisableMouseMove = useCallback(() => {
        if (moveCursorFinishedRef.current) {
            return false;
        } else {
            return true;
        }
    }, []);
    const enableMouseMove = useMemo(() => {
        return debounce(() => {
            moveCursorFinishedRef.current = true;
        }, 256);
    }, []);

    return useMemo(() => {
        return {
            disableMouseMove,
            isDisableMouseMove,
            enableMouseMove,
        };
    }, [disableMouseMove, enableMouseMove, isDisableMouseMove]);
};
