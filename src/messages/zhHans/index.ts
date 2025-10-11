import { menu } from './menu';
import { settings } from './settings';
import { home } from './home';
import { draw } from './draw';
import { tools } from './tools';
import { common } from './common';
import { about } from './about';
import { fullScreenDraw } from './fullScreenDraw';
import { videoRecord } from './videoRecord';
import { plugin } from './plugin';
import { personalization } from './personalization';

export const zhHans = {
    ...menu,
    ...settings,
    ...home,
    ...draw,
    ...tools,
    ...common,
    ...fullScreenDraw,
    ...about,
    ...videoRecord,
    ...plugin,
    ...personalization,
};
