import { ElementRect } from '@/commands';
import { MousePosition } from '@/utils/mousePosition';

export const dragRect = (
    rect: ElementRect,
    originMousePosition: MousePosition,
    currentMousePosition: MousePosition,
    previousRect: ElementRect | undefined,
    boundaryRect: ElementRect,
) => {
    const offsetX = currentMousePosition.mouseX - originMousePosition.mouseX;
    const offsetY = currentMousePosition.mouseY - originMousePosition.mouseY;

    const baseRect = previousRect || rect;

    let minX = baseRect.min_x + offsetX;
    let minY = baseRect.min_y + offsetY;
    let maxX = baseRect.max_x + offsetX;
    let maxY = baseRect.max_y + offsetY;

    const width = maxX - minX;
    const height = maxY - minY;

    // 检测是否超出边界（在调整之前）
    const isBeyond =
        minX < boundaryRect.min_x ||
        minY < boundaryRect.min_y ||
        maxX > boundaryRect.max_x ||
        maxY > boundaryRect.max_y;

    // 检测是否触碰边界
    const adjustedOriginPosition = new MousePosition(
        originMousePosition.mouseX,
        originMousePosition.mouseY,
    );
    let boundaryHit = false;

    if (minX < boundaryRect.min_x) {
        minX = boundaryRect.min_x;
        maxX = minX + width;
        // 调整原点X坐标，消除回弹效应
        adjustedOriginPosition.mouseX = currentMousePosition.mouseX - (minX - baseRect.min_x);
        boundaryHit = true;
    } else if (maxX > boundaryRect.max_x) {
        maxX = boundaryRect.max_x;
        minX = maxX - width;
        // 调整原点X坐标
        adjustedOriginPosition.mouseX = currentMousePosition.mouseX - (minX - baseRect.min_x);
        boundaryHit = true;
    }

    if (minY < boundaryRect.min_y) {
        minY = boundaryRect.min_y;
        maxY = minY + height;
        // 调整原点Y坐标
        adjustedOriginPosition.mouseY = currentMousePosition.mouseY - (minY - baseRect.min_y);
        boundaryHit = true;
    } else if (maxY > boundaryRect.max_y) {
        maxY = boundaryRect.max_y;
        minY = maxY - height;
        // 调整原点Y坐标
        adjustedOriginPosition.mouseY = currentMousePosition.mouseY - (minY - baseRect.min_y);
        boundaryHit = true;
    }

    return {
        rect: {
            min_x: minX,
            min_y: minY,
            max_x: maxX,
            max_y: maxY,
        },
        newOriginPosition: boundaryHit ? adjustedOriginPosition : originMousePosition,
        isBeyond,
    };
};

export const updateElementPosition = (
    element: HTMLElement,
    baseOffsetX: number,
    baseOffsetY: number,
    originMousePosition: MousePosition,
    currentMousePosition: MousePosition,
    previousRect: ElementRect | undefined,
    cancelOnBeyond: boolean = false,
    contentScale: number = 1,
    calculatedBoundaryRect: (rect: ElementRect) => ElementRect = (rect) => rect,
) => {
    let { clientWidth: toolbarWidth, clientHeight: toolbarHeight } = element;

    toolbarWidth = toolbarWidth * contentScale;
    toolbarHeight = toolbarHeight * contentScale;

    const viewportWidth = Math.max(document.body.clientWidth, toolbarWidth);
    const viewportHeight = Math.max(document.body.clientHeight, toolbarHeight);

    const dragRes = dragRect(
        {
            min_x: 0,
            min_y: 0,
            max_x: toolbarWidth,
            max_y: toolbarHeight,
        },
        originMousePosition,
        currentMousePosition,
        previousRect,
        calculatedBoundaryRect({
            min_x: -baseOffsetX,
            min_y: -baseOffsetY,
            max_x: -baseOffsetX + viewportWidth,
            max_y: -baseOffsetY + viewportHeight,
        }),
    );

    if (!(cancelOnBeyond && dragRes.isBeyond)) {
        const translateX = baseOffsetX + dragRes.rect.min_x;
        const translateY = baseOffsetY + dragRes.rect.min_y;

        element.style.transform = `translate(${translateX}px, ${translateY}px) scale(${contentScale})`;
    }

    return {
        rect: dragRes.rect,
        originPosition: dragRes.newOriginPosition,
        isBeyond: dragRes.isBeyond,
    };
};
