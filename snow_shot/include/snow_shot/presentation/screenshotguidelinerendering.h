#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTGUIDELINERENDERING_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTGUIDELINERENDERING_H

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include <cmath>

[[nodiscard]] inline qreal screenshotGuideLinePixelCenter(qreal coordinate) {
    return std::floor(coordinate) + 0.5;
}

inline void paintScreenshotGuideLineCrosshair(QPainter& painter, const QRectF& bounds,
                                              const QPointF& center, const QColor& color,
                                              bool dashed) {
    if (!color.isValid() || color.alpha() == 0 || bounds.isEmpty()) {
        return;
    }

    QPen pen(color, 1.0);
    pen.setCosmetic(true);
    if (dashed) {
        pen.setDashPattern(QVector<qreal>{10.0, 3.0});
    }

    const qreal left = screenshotGuideLinePixelCenter(bounds.left());
    const qreal top = screenshotGuideLinePixelCenter(bounds.top());
    const qreal right = std::ceil(bounds.right()) - 0.5;
    const qreal bottom = std::ceil(bounds.bottom()) - 0.5;
    const qreal x = screenshotGuideLinePixelCenter(center.x());
    const qreal y = screenshotGuideLinePixelCenter(center.y());

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(x, top), QPointF(x, bottom));
    painter.drawLine(QPointF(left, y), QPointF(right, y));
    painter.restore();
}

inline void paintScreenshotColorPickerCenterGuideLines(QPainter& painter, const QRectF& preview,
                                                       const QRectF& centerSample,
                                                       const QColor& color) {
    if (!color.isValid() || color.alpha() == 0 || preview.isEmpty() || centerSample.isEmpty()) {
        return;
    }

    QPen pen(color, 1.0);
    pen.setCosmetic(true);
    const qreal left = screenshotGuideLinePixelCenter(preview.left());
    const qreal top = screenshotGuideLinePixelCenter(preview.top());
    const qreal right = std::ceil(preview.right()) - 0.5;
    const qreal bottom = std::ceil(preview.bottom()) - 0.5;
    const qreal x = screenshotGuideLinePixelCenter(centerSample.center().x());
    const qreal y = screenshotGuideLinePixelCenter(centerSample.center().y());
    const qreal centerTop = centerSample.top() - 0.5;
    const qreal centerBottom = centerSample.bottom() + 0.5;
    const qreal centerLeft = centerSample.left() - 0.5;
    const qreal centerRight = centerSample.right() + 0.5;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (top <= centerTop) {
        painter.drawLine(QPointF(x, top), QPointF(x, centerTop));
    }
    if (centerBottom <= bottom) {
        painter.drawLine(QPointF(x, centerBottom), QPointF(x, bottom));
    }
    if (left <= centerLeft) {
        painter.drawLine(QPointF(left, y), QPointF(centerLeft, y));
    }
    if (centerRight <= right) {
        painter.drawLine(QPointF(centerRight, y), QPointF(right, y));
    }
    painter.restore();
}

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTGUIDELINERENDERING_H
