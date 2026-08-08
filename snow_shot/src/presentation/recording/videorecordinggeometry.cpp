#include "videorecordinggeometry.h"

#include <cmath>

namespace {
constexpr int kPhysicalBorderWidth = 2;

qreal validScale(qreal scale) {
    return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
}

} // namespace

namespace snow_shot::presentation::recording {
VideoRecordingAreaFrameGeometry videoRecordingAreaFrameGeometry(const QRectF& logicalRegion,
                                                                qreal physicalScale) {
    if (!logicalRegion.isValid() || logicalRegion.isEmpty()) {
        return {};
    }

    const qreal scale = validScale(physicalScale);
    const qreal borderWidth = static_cast<qreal>(kPhysicalBorderWidth) / scale;
    const QRectF frameGeometry =
        logicalRegion.adjusted(-borderWidth, -borderWidth, borderWidth, borderWidth);
    const QRect windowGeometry = frameGeometry.toAlignedRect();
    const QRectF selectionRect = logicalRegion.translated(-windowGeometry.topLeft());
    const QRectF frameRect =
        selectionRect.adjusted(-borderWidth, -borderWidth, borderWidth, borderWidth);

    return VideoRecordingAreaFrameGeometry{
        windowGeometry,
        frameRect,
        selectionRect,
        borderWidth,
    };
}

QRect videoRecordingCompatibleCaptureRegion(const QRect& selectedPhysicalRegion,
                                            const QRect& physicalBounds) {
    if (!selectedPhysicalRegion.isValid() || selectedPhysicalRegion.isEmpty()) {
        return {};
    }

    QRect captureRegion = selectedPhysicalRegion;
    if (captureRegion.width() % 2 != 0) {
        if (physicalBounds.isValid() && captureRegion.right() >= physicalBounds.right() &&
            captureRegion.left() > physicalBounds.left()) {
            captureRegion.setLeft(captureRegion.left() - 1);
        } else {
            captureRegion.setWidth(captureRegion.width() + 1);
        }
    }
    if (captureRegion.height() % 2 != 0) {
        if (physicalBounds.isValid() && captureRegion.bottom() >= physicalBounds.bottom() &&
            captureRegion.top() > physicalBounds.top()) {
            captureRegion.setTop(captureRegion.top() - 1);
        } else {
            captureRegion.setHeight(captureRegion.height() + 1);
        }
    }
    return captureRegion;
}
} // namespace snow_shot::presentation::recording
