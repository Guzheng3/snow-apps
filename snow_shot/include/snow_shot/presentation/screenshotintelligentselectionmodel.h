#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTINTELLIGENTSELECTIONMODEL_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTINTELLIGENTSELECTIONMODEL_H

#include <QPointF>
#include <QRectF>
#include <QVector>

class ScreenshotIntelligentSelectionModel final {
  public:
    void reset();
    void clearHitPath();
    void clearPress();

    [[nodiscard]] bool applyCanvasHitPath(const QVector<QRectF>& canvasHitRects,
                                          const QRectF& selectableBounds,
                                          qreal minimumSelectionSize);
    [[nodiscard]] bool setIndex(int index);
    [[nodiscard]] int index() const;
    [[nodiscard]] bool hasCurrentSelection() const;
    [[nodiscard]] QRectF currentSelection() const;

    void beginPress(const QPointF& position, const QRectF& selection);
    [[nodiscard]] bool pressActive() const;
    [[nodiscard]] QPointF pressPosition() const;
    [[nodiscard]] bool shouldStartManualDrag(const QPointF& position,
                                             double dragStartDistance) const;
    [[nodiscard]] QRectF takePressSelection();

  private:
    QVector<QRectF> m_hitRects;
    int m_index = -1;
    bool m_pressActive = false;
    QPointF m_pressPosition;
    QRectF m_pressSelection;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTINTELLIGENTSELECTIONMODEL_H
