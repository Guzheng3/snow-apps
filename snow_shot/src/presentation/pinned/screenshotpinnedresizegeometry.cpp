#include "screenshotpinnedresizegeometry.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {
using DragHandle = screenshot_pinned_resize_geometry::DragHandle;

bool isCornerHandle(DragHandle handle) {
    switch (handle) {
    case DragHandle::TopLeft:
    case DragHandle::TopRight:
    case DragHandle::BottomRight:
    case DragHandle::BottomLeft:
        return true;
    case DragHandle::Top:
    case DragHandle::Right:
    case DragHandle::Bottom:
    case DragHandle::Left:
        return false;
    }
    return false;
}

bool isHorizontalHandle(DragHandle handle) {
    return handle == DragHandle::Left || handle == DragHandle::Right;
}

double requestedScale(const QSize& proposed, const QSize& baseline, DragHandle handle) {
    const double widthScale = static_cast<double>(proposed.width()) / baseline.width();
    const double heightScale = static_cast<double>(proposed.height()) / baseline.height();

    if (isCornerHandle(handle)) {
        return std::max(widthScale, heightScale);
    }
    return isHorizontalHandle(handle) ? widthScale : heightScale;
}

void attachToFixedAnchor(QRect* rect, const QRect& reference, DragHandle handle) {
    switch (handle) {
    case DragHandle::TopLeft:
        rect->moveBottomRight(reference.bottomRight());
        break;
    case DragHandle::Top:
    case DragHandle::TopRight:
        rect->moveBottomLeft(reference.bottomLeft());
        break;
    case DragHandle::Right:
    case DragHandle::BottomRight:
    case DragHandle::Bottom:
        rect->moveTopLeft(reference.topLeft());
        break;
    case DragHandle::BottomLeft:
    case DragHandle::Left:
        rect->moveTopRight(reference.topRight());
        break;
    }
}
} // namespace

QSize screenshot_pinned_resize_geometry::scaledSize(const QSize& baseline, double scale) {
    if (!baseline.isValid() || baseline.isEmpty() || !std::isfinite(scale) || scale <= 0.0) {
        return {};
    }

    return QSize(std::max(1, qRound(baseline.width() * scale)),
                 std::max(1, qRound(baseline.height() * scale)));
}

bool screenshot_pinned_resize_geometry::proportionalResizeRect(
    const QRect& proposed, const QRect& reference, const QSize& baseline, DragHandle handle,
    double minimumScale, double maximumScale, QRect* result) {
    if (result == nullptr || !proposed.isValid() || proposed.isEmpty() || !reference.isValid() ||
        reference.isEmpty() || !baseline.isValid() || baseline.isEmpty() ||
        !std::isfinite(minimumScale) || !std::isfinite(maximumScale) || minimumScale <= 0.0 ||
        maximumScale < minimumScale) {
        return false;
    }

    const double scale =
        std::clamp(requestedScale(proposed.size(), baseline, handle), minimumScale, maximumScale);
    const QSize size = scaledSize(baseline, scale);
    if (!size.isValid() || size.isEmpty()) {
        return false;
    }

    QRect resized(proposed.topLeft(), size);
    attachToFixedAnchor(&resized, reference, handle);
    *result = resized;
    return true;
}
