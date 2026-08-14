#include "snow_shot/presentation/screenshotcolorpickercontroller.h"

#include "snow_shot/presentation/screenshotcolorpickerwidget.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotselectionlimits.h"
#include "snow_shot/presentation/screenshotoverlaycoordinator.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>

#include <algorithm>
#include <optional>

#if defined(Q_OS_WIN) || defined(_WIN32)
#include <qt_windows.h>
#endif

namespace {
constexpr int kSelectionOpacityTolerance = 4;
} // namespace

ScreenshotColorPickerController::ScreenshotColorPickerController(
    ScreenshotOverlayCoordinator& overlayCoordinator, const ScreenshotGeometryMapper& geometry,
    const ScreenshotDisplaySession& displaySession)
    : m_overlayCoordinator(overlayCoordinator), m_geometry(geometry),
      m_displaySession(displaySession) {}

void ScreenshotColorPickerController::reset() {
    m_overlay = nullptr;
    m_cursorPhysicalPoint = QPoint();
    m_hasCursorPhysicalPoint = false;
    m_suppressed = false;
}

void ScreenshotColorPickerController::hide() const {
    m_overlayCoordinator.hideColorPicker();
}

void ScreenshotColorPickerController::setSuppressed(bool suppressed) {
    if (m_suppressed == suppressed) {
        return;
    }

    m_suppressed = suppressed;
    if (m_suppressed) {
        hide();
    }
}

void ScreenshotColorPickerController::setDisplayMode(ScreenshotColorPickerDisplayMode mode) {
    if (m_displayMode == mode) {
        return;
    }
    m_displayMode = mode;
    if (m_displayMode == ScreenshotColorPickerDisplayMode::AlwaysHide) {
        hide();
    }
}

ScreenshotColorPickerDisplayMode ScreenshotColorPickerController::displayMode() const {
    return m_displayMode;
}

void ScreenshotColorPickerController::updateForOverlay(
    ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
    const ScreenshotColorPickerContext& context) {
    if (overlay == nullptr || !enabled(context)) {
        hide();
        return;
    }

    const QPointF virtualPosition =
        m_geometry.canvasPositionForOverlayLocalPoint(m_displaySession, overlay, localPosition);
    updateAtPhysicalPoint(physicalPositionForCanvasPoint(virtualPosition), context);
}

void ScreenshotColorPickerController::updateAtPhysicalPoint(
    const QPoint& physicalPoint, const ScreenshotColorPickerContext& context, qreal opacity) {
    updateAtPhysicalPoint(physicalPoint, context, opacity, true);
}

void ScreenshotColorPickerController::updateAtPhysicalPoint(
    const QPoint& physicalPoint, const ScreenshotColorPickerContext& context, qreal opacity,
    bool rememberAsCursor) {
    if (rememberAsCursor) {
        rememberCursorPhysicalPoint(physicalPoint);
    }

    if (!enabled(context) || screenshotUiContainsGlobalCursor()) {
        hide();
        return;
    }

    const CapturedDisplayModel* display = displayForPhysicalPoint(physicalPoint);
    ScreenshotOverlayWindow* overlay = m_displaySession.overlayForDisplay(display);
    if (display == nullptr || overlay == nullptr) {
        hide();
        return;
    }

    const QPoint logicalPoint = logicalPositionForPhysicalPoint(physicalPoint, *display);
    const QPointF overlayLocalPosition = QPointF(logicalPoint - overlay->geometry().topLeft());
    const qreal pickerOpacity = std::min(std::clamp<qreal>(opacity, 0.0, 1.0),
                                         opacityForPoint(physicalPoint, opacity < 1.0, context));

    m_overlayCoordinator.updateColorPicker(overlay, display->image, display->physicalRect,
                                           physicalPoint, overlayLocalPosition, pickerOpacity);
    m_overlay = overlay;
}

void ScreenshotColorPickerController::updateAtCurrentCursor(
    const ScreenshotColorPickerContext& context) {
    if (!enabled(context)) {
        hide();
        return;
    }

    updateAtPhysicalPoint(physicalPositionForLogicalPoint(QCursor::pos()), context);
}

void ScreenshotColorPickerController::updateForSelectionDrag(
    const QPointF& virtualPosition, const ScreenshotColorPickerContext& context) {
    if (!enabled(context) || !context.dragging) {
        return;
    }

    const QPoint cursorPhysicalPoint = physicalPositionForCanvasPoint(virtualPosition);
    const std::optional<QPointF> anchor =
        screenshotSelectionDragAnchor(context.selectionCanvas, context.dragMode, virtualPosition,
                                      snow_shot::presentation::kScreenshotSelectionMinimumSize);
    rememberCursorPhysicalPoint(cursorPhysicalPoint);
    if (!anchor.has_value()) {
        // Manual selection drags have no edge anchor, so sample at the actual
        // cursor while retaining the same cursor origin used by keyboard moves.
        updateAtPhysicalPoint(cursorPhysicalPoint, context, 1.0, false);
        return;
    }

    updateAtPhysicalPoint(physicalPositionForCanvasPoint(anchor.value()), context,
                          screenshotColorPickerOpacity(
                              m_displayMode,
                              ScreenshotColorPickerVisibilityState{
                                  context.intelligentSelecting,
                                  context.manualSelecting,
                                  context.movingSelection,
                                  context.dragging,
                                  true,
                                  context.selectionPixels.width() >= 1 &&
                                      context.selectionPixels.height() >= 1,
                                  true,
                              }),
                          false);
}

bool ScreenshotColorPickerController::copyColorToClipboard(
    const ScreenshotColorPickerContext& context) {
    ScreenshotColorPickerWidget* picker = m_overlayCoordinator.colorPicker();
    if (m_overlay == nullptr || picker == nullptr || !picker->hasCurrentColor() ||
        !enabled(context)) {
        return false;
    }

    QApplication::clipboard()->setText(picker->currentColorText());
    return true;
}

bool ScreenshotColorPickerController::cycleFormat(const ScreenshotColorPickerContext& context) {
    if (!enabled(context)) {
        return false;
    }

    ScreenshotColorPickerWidget* picker = m_overlayCoordinator.colorPicker();
    if (picker == nullptr) {
        return false;
    }
    picker->cycleColorFormat();
    return true;
}

bool ScreenshotColorPickerController::moveCursor(int dx, int dy,
                                                 const ScreenshotColorPickerContext& context) {
    if (!enabled(context) || m_displaySession.isEmpty()) {
        return false;
    }

    const QPoint currentPoint = m_hasCursorPhysicalPoint
                                    ? m_cursorPhysicalPoint
                                    : physicalPositionForLogicalPoint(QCursor::pos());
    const CapturedDisplayModel* currentDisplay = displayForPhysicalPoint(currentPoint);
    QPoint nextPoint = currentPoint;
    nextPoint += QPoint(dx, dy);
    nextPoint = m_geometry.clampPhysicalPointToDesktop(nextPoint);

    // The desktop bounds are a bounding rectangle. A multi-monitor desktop can
    // contain gaps inside that rectangle, so never move into a point that has
    // no captured display. Otherwise the picker briefly loses its owner and
    // flickers while Windows resolves the cursor to another monitor.
    if (displayForPhysicalPoint(nextPoint) == nullptr && currentDisplay != nullptr) {
        nextPoint = m_geometry.clampPhysicalPointToDisplay(*currentDisplay, nextPoint);
    }

    if (nextPoint == currentPoint) {
        rememberCursorPhysicalPoint(currentPoint);
        if (context.dragging) {
            updateForSelectionDrag(canvasPositionForPhysicalPoint(currentPoint), context);
        } else {
            updateAtPhysicalPoint(currentPoint, context);
        }
        return true;
    }

    if (!setPhysicalCursorPosition(nextPoint)) {
        return false;
    }

    if (context.dragging) {
        updateForSelectionDrag(canvasPositionForPhysicalPoint(nextPoint), context);
    } else {
        updateAtPhysicalPoint(nextPoint, context);
    }
    return true;
}

void ScreenshotColorPickerController::rememberCursorPhysicalPoint(const QPoint& physicalPoint) {
    m_cursorPhysicalPoint = physicalPoint;
    m_hasCursorPhysicalPoint = true;
}

bool ScreenshotColorPickerController::enabled(const ScreenshotColorPickerContext& context) const {
    return !m_suppressed && context.active && context.moveToolActive &&
           (context.intelligentSelecting || context.manualSelecting || context.movingSelection);
}

const CapturedDisplayModel*
ScreenshotColorPickerController::displayForPhysicalPoint(const QPointF& point) const {
    return m_geometry.displayForPhysicalPoint(m_displaySession, point);
}

QPoint ScreenshotColorPickerController::physicalPositionForLogicalPoint(
    const QPointF& logicalPoint) const {
    return m_geometry.physicalPositionForLogicalPoint(m_displaySession, logicalPoint);
}

QPoint ScreenshotColorPickerController::logicalPositionForPhysicalPoint(
    const QPointF& point, const CapturedDisplayModel& display) const {
    return m_geometry.logicalPositionForPhysicalPoint(display, point).toPoint();
}

QPoint ScreenshotColorPickerController::physicalPositionForCanvasPoint(const QPointF& point) const {
    return m_geometry.physicalPositionForCanvasPoint(m_displaySession, point);
}

QPointF
ScreenshotColorPickerController::canvasPositionForPhysicalPoint(const QPointF& point) const {
    return m_geometry.canvasPositionForPhysicalPoint(m_displaySession, point);
}

bool ScreenshotColorPickerController::screenshotUiContainsGlobalCursor() const {
    return m_overlayCoordinator.screenshotUiContainsGlobalCursor();
}

qreal ScreenshotColorPickerController::opacityForPoint(
    const QPoint& physicalPoint, bool selectionDrag,
    const ScreenshotColorPickerContext& context) const {
    const bool hasSelection =
        context.selectionPixels.width() >= 1 && context.selectionPixels.height() >= 1;
    const QRect toleratedSelection =
        context.selectionPixels.adjusted(-kSelectionOpacityTolerance, -kSelectionOpacityTolerance,
                                         kSelectionOpacityTolerance, kSelectionOpacityTolerance);
    return screenshotColorPickerOpacity(
        m_displayMode,
        ScreenshotColorPickerVisibilityState{
            context.intelligentSelecting,
            context.manualSelecting,
            context.movingSelection,
            context.dragging,
            selectionDrag,
            hasSelection,
            hasSelection && QRectF(toleratedSelection)
                                .contains(canvasPositionForPhysicalPoint(physicalPoint)),
        });
}

bool ScreenshotColorPickerController::setPhysicalCursorPosition(const QPoint& physicalPoint) const {
#if defined(Q_OS_WIN) || defined(_WIN32)
    return SetCursorPos(physicalPoint.x(), physicalPoint.y()) != 0;
#else
    const CapturedDisplayModel* display = displayForPhysicalPoint(physicalPoint);
    if (display == nullptr) {
        return false;
    }
    QCursor::setPos(logicalPositionForPhysicalPoint(physicalPoint, *display));
    return true;
#endif
}
