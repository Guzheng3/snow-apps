import { CanvasLayer, CaptureStep } from './types';
import { DrawLayerActionType } from './components/drawLayer';
import { SelectLayerActionType, SelectRectParams } from './components/selectLayer';
import { createPublisher } from '@/hooks/useStatePublisher';
import { BaseLayerEventActionType } from './components/baseLayer';
import { ScreenshotType } from '@/functions/screenshot';
import { OcrDetectResult } from '@/commands/ocr';
import { MonitorInfo } from '@/commands/core';
import { ElementRect, ImageBuffer } from '@/commands';
import { MousePosition } from '@/utils/mousePosition';
import Flatbush from 'flatbush';
import { last } from 'es-toolkit';
import { ColorInstance } from 'color';

export const switchLayer = (
    layer: CanvasLayer | undefined,
    drawLayerAction: DrawLayerActionType | undefined,
    selectLayerAction: SelectLayerActionType | undefined,
) => {
    let switchDraw = false;
    let switchSelect = false;

    switch (layer) {
        case CanvasLayer.Draw:
            switchDraw = true;
            break;
        case CanvasLayer.Select:
            switchSelect = true;
            break;
        default:
            break;
    }

    drawLayerAction?.setEnable(switchDraw);
    selectLayerAction?.setEnable(switchSelect);
};

export const getMonitorRect = (monitorInfo: MonitorInfo | undefined) => {
    return {
        min_x: 0,
        min_y: 0,
        max_x: monitorInfo?.monitor_width ?? 0,
        max_y: monitorInfo?.monitor_height ?? 0,
    };
};

export enum CaptureEvent {
    onExecuteScreenshot = 'onExecuteScreenshot',
    onCaptureImageBufferReady = 'onCaptureImageBufferReady',
    onCaptureReady = 'onCaptureReady',
    onCaptureLoad = 'onCaptureLoad',
    onCaptureFinish = 'onCaptureFinish',
}

export type CaptureEventParams =
    | {
          event: CaptureEvent.onExecuteScreenshot;
      }
    | {
          event: CaptureEvent.onCaptureImageBufferReady;
          params: {
              imageBuffer: ImageBuffer;
          };
      }
    | {
          event: CaptureEvent.onCaptureLoad;
          params: Parameters<BaseLayerEventActionType['onCaptureLoad']>;
      }
    | {
          event: CaptureEvent.onCaptureFinish;
      }
    | {
          event: CaptureEvent.onCaptureReady;
          params: Parameters<BaseLayerEventActionType['onCaptureReady']>;
      };

export const CaptureStepPublisher = createPublisher<CaptureStep>(CaptureStep.Select);
export const CaptureLoadingPublisher = createPublisher<boolean>(true);
export const CaptureEventPublisher = createPublisher<CaptureEventParams | undefined>(undefined);
export const ScreenshotTypePublisher = createPublisher<ScreenshotType>(ScreenshotType.Default);

export enum DrawEvent {
    OcrDetect = 0,
    ScrollScreenshot = 1,
    MoveCursor = 2,
    /** 选区所在的 monitor 发生变化，可能相同值重复触发 */
    ChangeMonitor = 3,
    /** 选区参数动画发生变化 */
    SelectRectParamsAnimationChange = 4,
    /** ColorPicker 颜色发生变化 */
    ColorPickerColorChange = 5,
}

export type DrawEventParams =
    | {
          event: DrawEvent.OcrDetect;
          params: {
              result: OcrDetectResult;
          };
      }
    | {
          event: DrawEvent.ScrollScreenshot;
          params: undefined;
      }
    | {
          event: DrawEvent.MoveCursor;
          params: {
              x: number;
              y: number;
          };
      }
    | {
          event: DrawEvent.ChangeMonitor;
          params: {
              monitorRect: ElementRect;
          };
      }
    | {
          event: DrawEvent.SelectRectParamsAnimationChange;
          params: {
              selectRectParams: SelectRectParams;
          };
      }
    | {
          event: DrawEvent.ColorPickerColorChange;
          params: {
              color: ColorInstance;
          };
      }
    | undefined;

export const DrawEventPublisher = createPublisher<DrawEventParams>(undefined, true);

export class CaptureBoundingBoxInfo {
    rect: ElementRect;
    width: number;
    height: number;
    mousePosition: MousePosition;
    monitorRectList: ElementRect[];
    monitorRTree: Flatbush;

    constructor(rect: ElementRect, monitorRectList: ElementRect[], mousePosition: MousePosition) {
        this.rect = rect;
        this.width = rect.max_x - rect.min_x;
        this.height = rect.max_y - rect.min_y;
        // 将显示器的鼠标位置转为相对截图窗口的鼠标位置
        this.mousePosition = new MousePosition(
            mousePosition.mouseX - rect.min_x,
            mousePosition.mouseY - rect.min_y,
        );
        this.monitorRectList = monitorRectList;
        this.monitorRTree = new Flatbush(monitorRectList.length);
        monitorRectList.forEach((rect) => {
            this.monitorRTree.add(rect.min_x, rect.min_y, rect.max_x, rect.max_y);
        });
        this.monitorRTree.finish();
    }

    /**
     * 将相对显示器的选区转换为相对于截图窗口的选区
     * @param rect 选区
     * @returns 相对于截图窗口的选区
     */
    transformMonitorRect(rect: ElementRect) {
        return {
            min_x: rect.min_x - this.rect.min_x,
            min_y: rect.min_y - this.rect.min_y,
            max_x: rect.max_x - this.rect.min_x,
            max_y: rect.max_y - this.rect.min_y,
        };
    }

    /**
     * 将相对于截图窗口的选区转换为相对于显示器的选区
     * @param rect 选区
     * @returns 相对于显示器的选区
     */
    transformWindowRect(rect: ElementRect) {
        return {
            min_x: rect.min_x + this.rect.min_x,
            min_y: rect.min_y + this.rect.min_y,
            max_x: rect.max_x + this.rect.min_x,
            max_y: rect.max_y + this.rect.min_y,
        };
    }

    getActiveMonitorRectList(selectedRect: ElementRect): ElementRect[] {
        const monitorRectIndexList = this.monitorRTree.search(
            selectedRect.min_x,
            selectedRect.min_y,
            selectedRect.max_x,
            selectedRect.max_y,
        );

        return monitorRectIndexList.map((index) => this.monitorRectList[index]);
    }

    getActiveMonitorRect(selectedRect: ElementRect) {
        const activeMonitorRectList = this.getActiveMonitorRectList(selectedRect);

        return (
            last(activeMonitorRectList) ?? {
                min_x: 0,
                min_y: 0,
                max_x: this.width,
                max_y: this.height,
            }
        );
    }
}
