import { AppSettingsData, AppSettingsGroup } from '@/app/contextWrap';
import { CaptureHistoryItem, CaptureHistoryStore } from './appStore';
import { ElementRect, ImageBuffer, ImageEncoder, saveFile } from '@/commands';
import { join as joinPath } from '@tauri-apps/api/path';
import path from 'path';
import { NonDeletedExcalidrawElement, Ordered } from '@mg-chao/excalidraw/element/types';
import { AppState } from '@mg-chao/excalidraw/types';
import { appError, appWarn } from './log';
import {
    copyFile,
    createDir,
    getAppConfigBaseDir,
    removeDir,
    removeFile,
    writeFile,
} from '@/commands/file';

const captureHistoryImagesDir = 'captureHistoryImages';

const getCaptureImageFilePath = (fileName: string) => {
    return `${captureHistoryImagesDir}/${fileName}`;
};

export const getCaptureHistoryImageAbsPath = async (fileName: string) => {
    return joinPath(await getAppConfigBaseDir(), getCaptureImageFilePath(fileName));
};

const dayDuration = 24 * 60 * 60 * 1000;
export enum HistoryValidDuration {
    /** 用于测试，不对外暴露 */
    Test = 1,
    Day = dayDuration,
    Week = 7 * dayDuration,
    Month = 30 * dayDuration,
    Forever = 0,
}

export class CaptureHistory {
    private store: CaptureHistoryStore;

    constructor() {
        this.store = new CaptureHistoryStore();
    }

    async init() {
        await this.store.init();
    }

    static generateCaptureHistoryItem(
        imageBuffer: ImageBuffer | CaptureHistoryItem,
        excalidrawElements: readonly Ordered<NonDeletedExcalidrawElement>[] | undefined,
        excalidrawAppState: Readonly<AppState> | undefined,
        selectedRect: ElementRect,
    ): CaptureHistoryItem {
        let fileExtension = '.webp';
        if ('encoder' in imageBuffer) {
            switch (imageBuffer.encoder) {
                case ImageEncoder.WebP:
                    fileExtension = '.webp';
                    break;
                case ImageEncoder.Png:
                    fileExtension = '.png';
                    break;
            }
        } else {
            fileExtension = path.extname(imageBuffer.file_name);
        }

        const timestamp = Date.now();
        const fileName = `${timestamp}${fileExtension}`;

        return {
            id: timestamp.toString(),
            selected_rect: selectedRect,
            file_name: fileName,
            create_ts: timestamp,
            excalidraw_elements: excalidrawElements,
            excalidraw_app_state: excalidrawAppState
                ? ({
                      zoom: excalidrawAppState.zoom,
                      scrollX: excalidrawAppState.scrollX,
                      scrollY: excalidrawAppState.scrollY,
                  } as CaptureHistoryItem['excalidraw_app_state'])
                : undefined,
        };
    }

    async save(
        imageData: ImageBuffer | CaptureHistoryItem,
        excalidrawElements: readonly Ordered<NonDeletedExcalidrawElement>[] | undefined,
        excalidrawAppState: Readonly<AppState> | undefined,
        selectedRect: ElementRect,
    ): Promise<CaptureHistoryItem> {
        const captureHistoryItem = CaptureHistory.generateCaptureHistoryItem(
            imageData,
            excalidrawElements,
            excalidrawAppState,
            selectedRect,
        );

        try {
            await createDir(await getCaptureHistoryImageAbsPath(''));

            if ('encoder' in imageData) {
                await writeFile(
                    await getCaptureHistoryImageAbsPath(captureHistoryItem.file_name),
                    new Uint8Array(imageData.buffer),
                );
            } else {
                await copyFile(
                    await getCaptureHistoryImageAbsPath(imageData.file_name),
                    await getCaptureHistoryImageAbsPath(captureHistoryItem.file_name),
                );
            }

            await this.store.set(captureHistoryItem.id, captureHistoryItem);
        } catch (error) {
            appError('[CaptureHistory] save captureHistoryItem failed', error);
        }

        return captureHistoryItem;
    }

    async getList(appSettings: AppSettingsData): Promise<CaptureHistoryItem[]> {
        const now = Date.now();
        const validTime =
            appSettings[AppSettingsGroup.SystemScreenshot].historyValidDuration ===
            HistoryValidDuration.Forever
                ? 0
                : now - appSettings[AppSettingsGroup.SystemScreenshot].historyValidDuration;

        const historyList = await this.store.entries().then((entries) => {
            return entries.filter(([, item]) => {
                return item.create_ts > validTime;
            });
        });
        return historyList
            .map(([, item]) => {
                return item;
            })
            .sort((a, b) => {
                // 按创建时间正序
                return a.create_ts - b.create_ts;
            });
    }

    async clearExpired(appSettings: AppSettingsData) {
        if (
            appSettings[AppSettingsGroup.SystemScreenshot].historyValidDuration ===
            HistoryValidDuration.Forever
        ) {
            return;
        }

        const historyList = await this.store.entries();

        const now = Date.now();
        const validTime = now - appSettings[AppSettingsGroup.SystemScreenshot].historyValidDuration;

        await Promise.all(
            historyList.map(async ([id, item]) => {
                if (item.create_ts > validTime) {
                    return;
                }

                try {
                    await this.store.delete(id);
                } catch (error) {
                    appWarn('[CaptureHistory] delete captureHistoryItem failed', error);
                }
                try {
                    await removeFile(await getCaptureHistoryImageAbsPath(item.file_name));
                } catch (error) {
                    appWarn('[CaptureHistory] remove captureHistoryItem image failed', error);
                }
            }),
        );
    }

    async clearAll() {
        await this.store.clear();
        try {
            await removeDir(await getCaptureHistoryImageAbsPath(''));
        } catch (error) {
            appWarn('[CaptureHistory] remove captureHistoryImagesDir failed', error);
        }
    }
}
