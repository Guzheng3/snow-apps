#include "snow_shot/presentation/screenshotintelligentselectionmodel.h"

#include <algorithm>

void ScreenshotIntelligentSelectionModel::reset() {
    clearHitPath();
    clearPress();
}

void ScreenshotIntelligentSelectionModel::clearHitPath() {
    m_hitRects.clear();
    m_index = -1;
}

void ScreenshotIntelligentSelectionModel::clearPress() {
    m_pressActive = false;
    m_pressPosition = QPointF();
    m_pressSelection = QRectF();
}

bool ScreenshotIntelligentSelectionModel::applyCanvasHitPath(const QVector<QRectF>& canvasHitRects,
                                                             const QRectF& selectableBounds,
                                                             qreal minimumSelectionSize) {
    if (canvasHitRects.isEmpty()) {
        clearHitPath();
        return false;
    }

    QVector<QRectF> boundedHitRects;
    boundedHitRects.reserve(canvasHitRects.size());
    for (const QRectF& hitRect : canvasHitRects) {
        const QRectF bounded = hitRect.intersected(selectableBounds);
        if (bounded.width() < minimumSelectionSize || bounded.height() < minimumSelectionSize) {
            continue;
        }
        if (!boundedHitRects.isEmpty() && bounded == boundedHitRects.constLast()) {
            continue;
        }
        boundedHitRects.push_back(bounded);
    }

    if (boundedHitRects.isEmpty()) {
        clearHitPath();
        return false;
    }

    if (boundedHitRects == m_hitRects) {
        return setIndex(m_index);
    }

    m_hitRects = boundedHitRects;
    return setIndex(0);
}

bool ScreenshotIntelligentSelectionModel::setIndex(int index) {
    if (m_hitRects.isEmpty()) {
        clearHitPath();
        return false;
    }

    const int maxIndex = static_cast<int>(m_hitRects.size() - 1);
    m_index = std::clamp(index, 0, maxIndex);
    return true;
}

bool ScreenshotIntelligentSelectionModel::cycleIndex() {
    if (m_hitRects.isEmpty()) {
        return false;
    }

    const int nextIndex = m_index < 0 ? 0 : (m_index + 1) % m_hitRects.size();
    const int previousIndex = m_index;
    if (!setIndex(nextIndex)) {
        return false;
    }
    return previousIndex != m_index;
}

int ScreenshotIntelligentSelectionModel::index() const {
    return m_index;
}

bool ScreenshotIntelligentSelectionModel::hasCurrentSelection() const {
    return m_index >= 0 && m_index < m_hitRects.size();
}

QRectF ScreenshotIntelligentSelectionModel::currentSelection() const {
    return hasCurrentSelection() ? m_hitRects.at(m_index) : QRectF();
}

void ScreenshotIntelligentSelectionModel::beginPress(const QPointF& position,
                                                     const QRectF& selection) {
    m_pressActive = true;
    m_pressPosition = position;
    m_pressSelection = selection;
}

bool ScreenshotIntelligentSelectionModel::pressActive() const {
    return m_pressActive;
}

QPointF ScreenshotIntelligentSelectionModel::pressPosition() const {
    return m_pressPosition;
}

bool ScreenshotIntelligentSelectionModel::shouldStartManualDrag(const QPointF& position,
                                                                double dragStartDistance) const {
    if (!m_pressActive) {
        return false;
    }

    const QPointF delta = position - m_pressPosition;
    if (dragStartDistance <= 0.0) {
        return true;
    }

    const double distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
    return distanceSquared >= dragStartDistance * dragStartDistance;
}

QRectF ScreenshotIntelligentSelectionModel::takePressSelection() {
    const QRectF selection = m_pressSelection;
    clearPress();
    return selection;
}
