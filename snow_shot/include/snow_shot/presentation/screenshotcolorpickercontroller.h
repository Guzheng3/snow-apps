#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCOLORPICKERCONTROLLER_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCOLORPICKERCONTROLLER_H

#include "snow_shot/presentation/screenshotselectiongeometry.h"
#include "snow_shot/presentation/screenshotuipreferences.h"

#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QRectF>
#include <QtGlobal>

class ScreenshotDisplaySession;
class ScreenshotGeometryMapper;
class ScreenshotOverlayCoordinator;
class ScreenshotOverlayWindow;
struct CapturedDisplayModel;

struct ScreenshotColorPickerContext {
    bool active = false;
    bool moveToolActive = false;
    bool intelligentSelecting = false;
    bool manualSelecting = false;
    bool movingSelection = false;
    bool dragging = false;
    QRect selectionPixels;
    QRectF selectionCanvas;
    ScreenshotSelectionDragMode dragMode = ScreenshotSelectionDragMode::None;
};

class ScreenshotColorPickerController final {
  public:
    ScreenshotColorPickerController(ScreenshotOverlayCoordinator& overlayCoordinator,
                                    const ScreenshotGeometryMapper& geometry,
                                    const ScreenshotDisplaySession& displaySession);

    void reset();
    void hide() const;
    void setSuppressed(bool suppressed);
    void setDisplayMode(ScreenshotColorPickerDisplayMode mode);
    [[nodiscard]] ScreenshotColorPickerDisplayMode displayMode() const;
    void updateForOverlay(ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
                          const ScreenshotColorPickerContext& context);
    void updateAtPhysicalPoint(const QPoint& physicalPoint,
                               const ScreenshotColorPickerContext& context, qreal opacity = 1.0);
    void updateAtCurrentCursor(const ScreenshotColorPickerContext& context);
    void updateForSelectionDrag(const QPointF& virtualPosition,
                                const ScreenshotColorPickerContext& context);

    [[nodiscard]] bool copyColorToClipboard(const ScreenshotColorPickerContext& context);
    [[nodiscard]] bool cycleFormat(const ScreenshotColorPickerContext& context);
    [[nodiscard]] bool moveCursor(int dx, int dy, const ScreenshotColorPickerContext& context);
    [[nodiscard]] bool enabled(const ScreenshotColorPickerContext& context) const;

  private:
    [[nodiscard]] const CapturedDisplayModel* displayForPhysicalPoint(const QPointF& point) const;
    [[nodiscard]] QPoint physicalPositionForLogicalPoint(const QPointF& logicalPoint) const;
    [[nodiscard]] QPoint logicalPositionForPhysicalPoint(const QPointF& point,
                                                         const CapturedDisplayModel& display) const;
    [[nodiscard]] QPoint physicalPositionForCanvasPoint(const QPointF& point) const;
    [[nodiscard]] QPointF canvasPositionForPhysicalPoint(const QPointF& point) const;
    [[nodiscard]] bool screenshotUiContainsGlobalCursor() const;
    [[nodiscard]] qreal opacityForPoint(const QPoint& physicalPoint, bool selectionDrag,
                                        const ScreenshotColorPickerContext& context) const;
    [[nodiscard]] bool setPhysicalCursorPosition(const QPoint& physicalPoint) const;

    ScreenshotOverlayCoordinator& m_overlayCoordinator;
    const ScreenshotGeometryMapper& m_geometry;
    const ScreenshotDisplaySession& m_displaySession;
    QPointer<ScreenshotOverlayWindow> m_overlay;
    QPoint m_physicalPoint;
    bool m_hasPhysicalPoint = false;
    bool m_suppressed = false;
    ScreenshotColorPickerDisplayMode m_displayMode =
        ScreenshotColorPickerDisplayMode::HideOutsideSelection;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCOLORPICKERCONTROLLER_H
