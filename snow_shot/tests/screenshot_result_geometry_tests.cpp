#include "snow_shot/presentation/screenshotgeometry.h"

#include <cstdlib>
#include <iostream>
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
} // namespace

int main() {
    try {
        landscapeAndPortraitResultsFitBothAxes();
        fitUsesAvailableGeometryMarginAndNeverUpscales();
        fitDoesNotDropBelowMinimumZoom();
        fullResolutionPlacementCentersWithoutFitting();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
