#include "snow_shot/presentation/screenshotcursornavigator.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"

#include <QVector>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void landscapeAndPortraitResultsFitBothAxes() {
    const QRect logicalScreen(100, 50, 1920, 1080);
    const QRect available(100, 50, 1920, 1040);
    const QRect nativeScreen(100, 50, 2880, 1620);

    const ScreenshotPinnedImageFit landscape =
        ScreenshotGeometryMapper::fitImageToAvailableGeometry(
            QSize(5000, 1000), available, logicalScreen, nativeScreen);
    require(landscape.valid && landscape.nativeGeometry.width() <= 2832 &&
                landscape.nativeGeometry.height() <= 1512,
            "landscape result did not fit the inset work area");

    const ScreenshotPinnedImageFit portrait =
        ScreenshotGeometryMapper::fitImageToAvailableGeometry(
            QSize(800, 5000), available, logicalScreen, nativeScreen);
    require(portrait.valid && portrait.nativeGeometry.width() <= 2832 &&
                portrait.nativeGeometry.height() <= 1512,
            "portrait result did not fit the inset work area");
}

void fitUsesAvailableGeometryMarginAndNeverUpscales() {
    const ScreenshotPinnedImageFit small =
        ScreenshotGeometryMapper::fitImageToAvailableGeometry(
            QSize(320, 200), QRect(0, 0, 1280, 680), QRect(0, 0, 1280, 720),
            QRect(0, 0, 1600, 900));
    require(small.valid && small.nativeGeometry.size() == QSize(320, 200) &&
                small.scalePercent == 100.0,
            "small result was initially upscaled");

    const QRect insetNative = ScreenshotGeometryMapper::nativeRectForLogicalRect(
        QRect(16, 16, 1248, 648), QRect(0, 0, 1280, 720), QRect(0, 0, 1600, 900));
    require(insetNative.contains(small.nativeGeometry),
            "result is not centered inside the taskbar-safe inset");
}

void fitDoesNotDropBelowMinimumZoom() {
    const ScreenshotPinnedImageFit huge = ScreenshotGeometryMapper::fitImageToAvailableGeometry(
        QSize(10000, 10000), QRect(0, 0, 1280, 680), QRect(0, 0, 1280, 720),
        QRect(0, 0, 1600, 900));
    require(huge.valid && huge.scalePercent == 10.0 &&
                huge.fullResolutionSize == QSize(10000, 10000) &&
                huge.nativeGeometry.size() == QSize(1000, 1000),
            "adaptive fit dropped below the minimum zoom");
}

void fullResolutionPlacementCentersWithoutFitting() {
    const QRect logicalScreen(100, 50, 1600, 900);
    const QRect available(100, 50, 1600, 860);
    const QRect nativeScreen(200, 100, 2400, 1350);
    const QSize imageSize(5000, 3000);
    const ScreenshotPinnedImageFit placement =
        ScreenshotGeometryMapper::centerImageAtFullResolution(
            imageSize, available, logicalScreen, nativeScreen);
    const QRect availableNative = ScreenshotGeometryMapper::nativeRectForLogicalRect(
        available, logicalScreen, nativeScreen);
    const QPointF availableCenter(availableNative.left() + availableNative.width() / 2.0,
                                  availableNative.top() + availableNative.height() / 2.0);
    const QPointF placementCenter(
        placement.nativeGeometry.left() + placement.nativeGeometry.width() / 2.0,
        placement.nativeGeometry.top() + placement.nativeGeometry.height() / 2.0);

    require(placement.valid && placement.nativeGeometry.size() == imageSize &&
                placement.fullResolutionSize == imageSize && placement.scalePercent == 100.0,
            "full-resolution placement should preserve the image pixel dimensions");
    require((placementCenter - availableCenter).manhattanLength() <= 1.0 &&
                !availableNative.contains(placement.nativeGeometry),
            "full-resolution placement should center in the work area while allowing overflow");
}

void cursorPanelPlacementUsesEveryAvailableQuadrant() {
    constexpr int gap = 16;
    const QRect bounds(0, 0, 800, 600);
    const QSize panelSize(160, 220);

    require(ScreenshotGeometryMapper::cursorPanelPosition(QPoint(100, 100), panelSize, bounds,
                                                          gap) == QPoint(116, 116),
            "cursor panel should prefer the bottom-right position");
    require(ScreenshotGeometryMapper::cursorPanelPosition(QPoint(700, 100), panelSize, bounds,
                                                          gap) == QPoint(524, 116),
            "cursor panel should move to bottom-left when the right side is too narrow");
    require(ScreenshotGeometryMapper::cursorPanelPosition(QPoint(100, 500), panelSize, bounds,
                                                          gap) == QPoint(116, 264),
            "cursor panel should move to top-right when the bottom side is too short");
    require(ScreenshotGeometryMapper::cursorPanelPosition(QPoint(700, 500), panelSize, bounds,
                                                          gap) == QPoint(524, 264),
            "cursor panel should move to top-left when the right and bottom sides are too small");
}

void cursorPanelPlacementRespectsOffsetMonitorBounds() {
    constexpr int gap = 14;
    const QRect bounds(-1920, -160, 1920, 1080);
    const QSize panelSize(228, 84);

    require(ScreenshotGeometryMapper::cursorPanelPosition(QPoint(-10, 900), panelSize, bounds,
                                                          gap) == QPoint(-252, 802),
            "cursor panel should flip above-left inside an offset monitor");
    const QPoint clamped = ScreenshotGeometryMapper::cursorPanelPosition(
        QPoint(-1918, -158), QSize(2400, 1200), bounds, gap);
    require(clamped == bounds.topLeft(),
            "an oversized cursor panel should clamp to an offset monitor origin");
}

ScreenshotDisplaySession capturedDesktopWithGap() {
    ScreenshotDisplaySession displays;
    CapturedDisplayModel left;
    left.physicalRect = QRect(100, 200, 100, 80);
    left.active = true;
    displays.appendDisplay(left);

    CapturedDisplayModel right;
    right.physicalRect = QRect(220, 200, 100, 80);
    right.active = true;
    displays.appendDisplay(right);
    return displays;
}

void cursorNudgesAlwaysStartFromTheLivePosition() {
    ScreenshotDisplaySession displays = capturedDesktopWithGap();
    ScreenshotGeometryMapper geometry;

    QPoint livePosition(140, 230);
    int readCount = 0;
    QVector<QPoint> writes;
    ScreenshotCursorNavigator navigator(
        geometry, displays,
        [&livePosition, &readCount]() -> std::optional<QPoint> {
            ++readCount;
            return livePosition;
        },
        [&livePosition, &writes](const QPoint& position) {
            writes.push_back(position);
            livePosition = position;
            return true;
        });

    require(navigator.moveBy(QPoint(1, 0)) == QPoint(141, 230),
            "the first nudge did not start from the live cursor position");

    // Simulate mouse motion handled by the drawing canvas without reporting it
    // to the navigator. The old color-picker cache failed this exact sequence.
    livePosition = QPoint(170, 245);
    require(navigator.moveBy(QPoint(0, -1)) == QPoint(170, 244) && readCount == 2 &&
                writes == QVector<QPoint>{QPoint(141, 230), QPoint(170, 244)},
            "a nudge reused a stale position after drawing-tool mouse motion");
}

void cursorNudgesRespectMonitorGapsAndAccessFailures() {
    ScreenshotDisplaySession displays = capturedDesktopWithGap();
    ScreenshotGeometryMapper geometry;

    QPoint livePosition(199, 230);
    int writeCount = 0;
    ScreenshotCursorNavigator navigator(
        geometry, displays, [&livePosition]() { return std::optional<QPoint>(livePosition); },
        [&writeCount](const QPoint&) {
            ++writeCount;
            return true;
        });
    require(navigator.moveBy(QPoint(1, 0)) == QPoint(199, 230) && writeCount == 0,
            "a nudge entered the uncaptured gap to the right of a display");
    livePosition = QPoint(220, 230);
    require(navigator.moveBy(QPoint(-1, 0)) == QPoint(220, 230) && writeCount == 0,
            "a nudge entered the uncaptured gap to the left of a display");

    ScreenshotCursorNavigator readFailure(
        geometry, displays, []() -> std::optional<QPoint> { return std::nullopt; },
        [](const QPoint&) { return true; });
    require(!readFailure.moveBy(QPoint(1, 0)).has_value(),
            "a failed cursor read was reported as a successful nudge");

    ScreenshotCursorNavigator writeFailure(
        geometry, displays, []() { return std::optional<QPoint>(QPoint(150, 230)); },
        [](const QPoint&) { return false; });
    require(!writeFailure.moveBy(QPoint(1, 0)).has_value(),
            "a failed cursor write was reported as a successful nudge");
}
} // namespace

int main() {
    try {
        landscapeAndPortraitResultsFitBothAxes();
        fitUsesAvailableGeometryMarginAndNeverUpscales();
        fitDoesNotDropBelowMinimumZoom();
        fullResolutionPlacementCentersWithoutFitting();
        cursorPanelPlacementUsesEveryAvailableQuadrant();
        cursorPanelPlacementRespectsOffsetMonitorBounds();
        cursorNudgesAlwaysStartFromTheLivePosition();
        cursorNudgesRespectMonitorGapsAndAccessFailures();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
