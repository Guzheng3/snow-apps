import { appInfo } from '@/utils/log';

export enum VideoRecordState {
    Idle,
    Recording,
    Paused,
}

export const getVideoRecordParams = () => {
    const urlParams = new URLSearchParams(window.location.search);

    appInfo(
        `urlParams,select_rect_min_x:${urlParams.get('select_rect_min_x')},select_rect_min_y:${urlParams.get('select_rect_min_y')},select_rect_max_x:${urlParams.get('select_rect_max_x')},select_rect_max_y:${urlParams.get('select_rect_max_y')}`,
    );

    appInfo(
        `parseInt urlParams,select_rect_min_x:${parseInt(urlParams.get('select_rect_min_x') ?? '0')},select_rect_min_y:${parseInt(urlParams.get('select_rect_min_y') ?? '0')},select_rect_max_x:${parseInt(urlParams.get('select_rect_max_x') ?? '0')},select_rect_max_y:${parseInt(urlParams.get('select_rect_max_y') ?? '0')}`,
    );
    const selectRect = {
        min_x: parseInt(urlParams.get('select_rect_min_x') ?? '0'),
        min_y: parseInt(urlParams.get('select_rect_min_y') ?? '0'),
        max_x: parseInt(urlParams.get('select_rect_max_x') ?? '0'),
        max_y: parseInt(urlParams.get('select_rect_max_y') ?? '0'),
    };

    return {
        selectRect,
    };
};
