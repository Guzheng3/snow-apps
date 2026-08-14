#include "snow_shot/presentation/screenshotoverlayinputhandler.h"

#include "snow_shot/presentation/screenshotcapturestate.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotintelligentselectionmodel.h"
#include "snow_shot/presentation/screenshotselectionlimits.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"
#include "snow_shot/storage/settingsadapters.h"

#include <QApplication>
#include <QKeyCombination>
#include <QKeySequence>

#include <algorithm>
#include <utility>

namespace {
constexpr qreal kSelectionEdgeTolerance = 8.0;

bool wheelAdjustsStrokeWidth(ScreenshotActiveTool tool) {
    switch (tool) {
    case ScreenshotActiveTool::Shape:
    case ScreenshotActiveTool::Arrow:
    case ScreenshotActiveTool::Line:
    case ScreenshotActiveTool::FreeDraw:
    case ScreenshotActiveTool::RectangleHighlight:
    case ScreenshotActiveTool::PenHighlight:
        return true;
    default:
        return false;
    }
}

bool recognitionTool(ScreenshotActiveTool tool) {
    return tool == ScreenshotActiveTool::Ocr || tool == ScreenshotActiveTool::Table ||
           tool == ScreenshotActiveTool::Qr;
}

bool screenshotCompletionGestureTool(ScreenshotActiveTool tool) {
    return tool != ScreenshotActiveTool::Select && tool != ScreenshotActiveTool::Ocr &&
           tool != ScreenshotActiveTool::Table && tool != ScreenshotActiveTool::Qr;
}

QString portableShortcutForKeyPress(int key, Qt::KeyboardModifiers modifiers) {
    return QKeySequence(QKeyCombination(modifiers, Qt::Key(key)))
        .toString(QKeySequence::PortableText)
        .trimmed();
}

bool shortcutListContains(const QStringList& shortcuts, const QString& pressed) {
    return std::any_of(shortcuts.cbegin(), shortcuts.cend(), [&pressed](const QString& shortcut) {
        return shortcut.compare(pressed, Qt::CaseInsensitive) == 0;
    });
}
} // namespace

ScreenshotOverlayInputHandler::ScreenshotOverlayInputHandler(
    ScreenshotOverlayInputHandlerContext context)
    : m_context(std::move(context)) {}

void ScreenshotOverlayInputHandler::handleMousePress(ScreenshotOverlayWindow* overlay,
                                                     const QPointF& localPosition) {
    if (recognitionTool(m_context.interaction.activeTool())) {
        return;
    }
    updateGuideLines(overlay, localPosition);
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
    const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
    if (m_context.interaction.movingSelection()) {
        if (dragModeForVirtualPosition(virtualPosition, false) !=
            ScreenshotSelectionDragMode::None) {
            handleMovingSelectionPress(overlay, virtualPosition);
        } else {
            // Move also acts as the selection tool when the new box starts
            // outside the currently confirmed selection.
            handleManualSelectionPress(overlay, localPosition, virtualPosition);
        }
        return;
    }

    if (m_context.interaction.intelligentSelecting()) {
        handleIntelligentSelectionPress(virtualPosition);
        return;
    }

    if (!m_context.interaction.manualSelecting()) {
        return;
    }
    handleManualSelectionPress(overlay, localPosition, virtualPosition);
}

void ScreenshotOverlayInputHandler::handleMovingSelectionPress(ScreenshotOverlayWindow* overlay,
                                                               const QPointF& virtualPosition) {
    const ScreenshotSelectionDragMode dragMode = dragModeForVirtualPosition(virtualPosition, false);
    if (!m_context.interaction.enterMovingSelectionDrag(dragMode)) {
        return;
    }
    m_context.selection.beginMoveDrag(virtualPosition);
    m_context.actions.setOverlayCursor(overlay, dragMode);
    m_context.actions.hideMainToolbar();
    m_context.actions.updateColorPickerForSelectionDrag(virtualPosition);
}

void ScreenshotOverlayInputHandler::handleIntelligentSelectionPress(
    const QPointF& virtualPosition) {
    m_context.intelligentSelection.beginPress(virtualPosition,
                                              m_context.intelligentSelection.currentSelection());
}

void ScreenshotOverlayInputHandler::handleManualSelectionPress(ScreenshotOverlayWindow* overlay,
                                                               const QPointF& localPosition,
                                                               const QPointF& virtualPosition) {
    m_context.interaction.enterManualSelectionDrag();
    m_context.actions.pauseIntelligentSelection();
    m_context.selection.setSelectionStartEnd(virtualPosition, virtualPosition);
    m_context.actions.updateOverlayState();
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

bool ScreenshotOverlayInputHandler::shouldHandleMouseEvent(const ScreenshotOverlayWindow*,
                                                           const QPointF&, bool) const {
    if (m_context.interaction.scrollingCapture()) {
        return false;
    }
    if (recognitionTool(m_context.interaction.activeTool())) {
        return true;
    }
    if (m_context.interaction.selecting()) {
        return true;
    }
    if (m_context.interaction.dragging()) {
        return true;
    }
    if (m_context.interaction.movingSelection()) {
        // A confirmed selection can be replaced by dragging a new box from
        // any point on the capture surface, not only by grabbing its edge.
        return true;
    }
    return false;
}

void ScreenshotOverlayInputHandler::handleMouseMove(ScreenshotOverlayWindow* overlay,
                                                    const QPointF& localPosition) {
    if (recognitionTool(m_context.interaction.activeTool())) {
        return;
    }
    updateGuideLines(overlay, localPosition);
    if (m_context.interaction.intelligentSelecting()) {
        const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
        handleIntelligentSelectionMove(overlay, localPosition, virtualPosition);
        return;
    }

    if (!m_context.interaction.dragging()) {
        handleHoverMove(overlay, localPosition);
        return;
    }

    const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
    if (m_context.interaction.movingSelection()) {
        handleMovingSelectionDragMove(virtualPosition);
        return;
    }
    if (m_context.interaction.manualSelecting()) {
        handleManualSelectionDragMove(overlay, localPosition, virtualPosition);
    }
}

void ScreenshotOverlayInputHandler::handleIntelligentSelectionMove(ScreenshotOverlayWindow* overlay,
                                                                   const QPointF& localPosition,
                                                                   const QPointF& virtualPosition) {
    if (m_context.intelligentSelection.pressActive()) {
        if (m_context.intelligentSelection.shouldStartManualDrag(
                virtualPosition, QApplication::startDragDistance())) {
            const QPointF pressPosition = m_context.intelligentSelection.pressPosition();
            m_context.intelligentSelection.clearPress();
            m_context.interaction.enterManualSelectionDragFromIntelligent();
            m_context.actions.pauseIntelligentSelection();
            m_context.selection.setSelectionStartEnd(pressPosition, virtualPosition);
            m_context.intelligentSelection.clearHitPath();
            m_context.actions.hideMainToolbar();
            m_context.actions.updateOverlayState();
            m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
        }
        return;
    }

    requestIntelligentSelectionHitTest(virtualPosition);
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

void ScreenshotOverlayInputHandler::handleHoverMove(ScreenshotOverlayWindow* overlay,
                                                    const QPointF& localPosition) {
    if (m_context.interaction.movingSelection()) {
        const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
        m_context.actions.setOverlayCursor(overlay,
                                           dragModeForVirtualPosition(virtualPosition, false));
    }
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

void ScreenshotOverlayInputHandler::handleMovingSelectionDragMove(const QPointF& virtualPosition) {
    const QRectF dragged = selectionRectForDrag(m_context.interaction.dragMode(), virtualPosition);
    m_context.selection.setSelectionRect(dragged);
    m_context.actions.updateOverlayState();
    m_context.actions.updateColorPickerForSelectionDrag(virtualPosition);
}

void ScreenshotOverlayInputHandler::handleManualSelectionDragMove(ScreenshotOverlayWindow* overlay,
                                                                  const QPointF& localPosition,
                                                                  const QPointF& virtualPosition) {
    m_context.selection.setSelectionEnd(virtualPosition);
    m_context.actions.updateOverlayState();
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

void ScreenshotOverlayInputHandler::handleMouseRelease(ScreenshotOverlayWindow* overlay,
                                                       const QPointF& localPosition) {
    const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
    if (recognitionTool(m_context.interaction.activeTool())) {
        return;
    }
    if (m_context.interaction.intelligentSelecting()) {
        handleIntelligentSelectionRelease(virtualPosition);
        return;
    }

    if (!m_context.interaction.dragging()) {
        return;
    }
    if (m_context.interaction.movingSelection()) {
        handleMovingSelectionRelease(overlay, localPosition, virtualPosition);
        return;
    }
    if (m_context.interaction.manualSelecting()) {
        handleManualSelectionRelease(overlay, localPosition, virtualPosition);
    }
}

void ScreenshotOverlayInputHandler::handleIntelligentSelectionRelease(
    const QPointF& virtualPosition) {
    if (!m_context.intelligentSelection.pressActive()) {
        return;
    }

    const QRectF pressSelection = m_context.intelligentSelection.takePressSelection();
    if (pressSelection.isValid() && !pressSelection.isEmpty() &&
        pressSelection.contains(virtualPosition)) {
        m_context.selection.setSelectionRect(pressSelection);
        confirmSelection();
        return;
    }

    requestIntelligentSelectionHitTest(virtualPosition);
}

void ScreenshotOverlayInputHandler::handleMovingSelectionRelease(ScreenshotOverlayWindow* overlay,
                                                                 const QPointF& localPosition,
                                                                 const QPointF& virtualPosition) {
    const QRectF dragged = selectionRectForDrag(m_context.interaction.dragMode(), virtualPosition);
    m_context.selection.setSelectionRect(dragged);
    m_context.interaction.finishDrag();
    m_context.actions.updateOverlayState();
    m_context.actions.showToolbar();
    m_context.actions.showSelectionToolbar();
    m_context.actions.setOverlayCursor(
        overlay, dragModeForVirtualPosition(virtualPosition, m_context.interaction.editing()));
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

void ScreenshotOverlayInputHandler::handleManualSelectionRelease(ScreenshotOverlayWindow* overlay,
                                                                 const QPointF& localPosition,
                                                                 const QPointF& virtualPosition) {
    m_context.selection.setSelectionEnd(virtualPosition);
    m_context.interaction.finishDrag();
    confirmSelection();
    if (m_context.interaction.manualSelecting()) {
        m_context.actions.updateOverlayState();
    }
    m_context.actions.updateColorPickerForOverlay(overlay, localPosition);
}

bool ScreenshotOverlayInputHandler::handleRightClick(ScreenshotOverlayWindow* overlay,
                                                     const QPointF& localPosition) {
    if (recognitionTool(m_context.interaction.activeTool())) {
        return true;
    }
    if (!m_context.interaction.moveToolActive()) {
        return false;
    }

    const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
    const QPoint physicalPoint = physicalPositionForCanvasPoint(virtualPosition);
    if (m_context.interaction.intelligentSelecting()) {
        m_context.actions.cancelCapture();
        return true;
    }

    if (m_context.interaction.manualSelecting() || m_context.interaction.movingSelection()) {
        if (m_context.actions.returnToCurrentScreenshot()) {
            return true;
        }
        m_context.actions.returnToIntelligentSelection(physicalPoint);
        return true;
    }

    return false;
}

bool ScreenshotOverlayInputHandler::handleWheel(ScreenshotOverlayWindow* overlay,
                                                const QPointF& localPosition,
                                                const QPoint& angleDelta,
                                                const QPoint& pixelDelta) {
    if (m_context.interaction.scrollingCapture()) {
        return false;
    }
    if (recognitionTool(m_context.interaction.activeTool())) {
        return true;
    }
    const int deltaY = !pixelDelta.isNull() ? pixelDelta.y() : angleDelta.y();
    if (deltaY != 0 && wheelAdjustsStrokeWidth(m_context.interaction.activeTool())) {
        return m_context.actions.stepStrokeWidth(deltaY > 0 ? 1 : -1);
    }
    if (m_context.interaction.activeTool() == ScreenshotActiveTool::Select && deltaY != 0) {
        return m_context.actions.stepSelectionOpacity(deltaY > 0 ? 1 : -1);
    }
    if (m_context.interaction.activeTool() == ScreenshotActiveTool::Spotlight && deltaY != 0) {
        return m_context.actions.stepSpotlightOpacity(deltaY > 0 ? 1 : -1);
    }
    if (m_context.interaction.activeTool() == ScreenshotActiveTool::RectangleFilter &&
        deltaY != 0) {
        return m_context.actions.stepFilterIntensity(deltaY > 0 ? 1 : -1);
    }
    if (m_context.interaction.activeTool() == ScreenshotActiveTool::PenFilter && deltaY != 0) {
        return m_context.actions.stepPenFilterStrokeWidth(deltaY > 0 ? 1 : -1);
    }
    if (m_context.interaction.activeTool() == ScreenshotActiveTool::Watermark && deltaY != 0) {
        return m_context.actions.stepWatermarkFontSize(deltaY > 0 ? 1 : -1);
    }

    if (!m_context.interaction.intelligentSelecting()) {
        return false;
    }

    const QPointF virtualPosition = virtualPositionForOverlay(overlay, localPosition);
    requestIntelligentSelectionHitTest(virtualPosition);

    if (deltaY > 0) {
        setIntelligentSelectionIndex(m_context.intelligentSelection.index() + 1);
    } else if (deltaY < 0) {
        setIntelligentSelectionIndex(m_context.intelligentSelection.index() - 1);
    }

    m_context.actions.updateOverlayState();
    return true;
}

bool ScreenshotOverlayInputHandler::handleKeyPress(int key, Qt::KeyboardModifiers modifiers) {
    if (recognitionTool(m_context.interaction.activeTool())) {
        return true;
    }
    if (key == Qt::Key_Escape) {
        m_context.actions.cancelCapture();
        return true;
    }
    const bool controlPressed = modifiers.testFlag(Qt::ControlModifier);
    const bool altPressed = modifiers.testFlag(Qt::AltModifier);
    const bool metaPressed = modifiers.testFlag(Qt::MetaModifier);
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && m_context.interaction.selecting()) {
        confirmSelection();
        return true;
    }

    if (modifiers == Qt::KeyboardModifiers() &&
        (m_context.interaction.selecting() || m_context.interaction.movingSelection()) &&
        (key == Qt::Key_Comma || key == Qt::Key_Period)) {
        if (m_context.interaction.manualSelecting()) {
            if (m_context.interaction.dragging()) {
                m_context.interaction.cancelDrag();
            }
            confirmSelection();
        }
        m_context.actions.pauseIntelligentSelection();
        m_context.intelligentSelection.clearPress();
        if (key == Qt::Key_Comma) {
            static_cast<void>(m_context.actions.navigateHistoryPrevious());
        } else {
            static_cast<void>(m_context.actions.navigateHistoryNext());
        }
        return true;
    }

    if (key == Qt::Key_C && controlPressed && !altPressed && !metaPressed) {
        m_context.actions.copySelectionToClipboard();
        return true;
    }

    if (handleScreenshotShortcut(key, modifiers)) {
        return true;
    }
    if (handleDrawingShortcut(key, modifiers)) {
        return true;
    }
    return m_context.interaction.moveToolActive() &&
           handleColorPickerKeyPress(key, modifiers);
}

bool ScreenshotOverlayInputHandler::handleScreenshotShortcut(
    int key, Qt::KeyboardModifiers modifiers) {
    if (!(m_context.interaction.movingSelection() || m_context.interaction.editing()) ||
        !m_context.actions.localShortcutInputAllowed()) {
        return false;
    }

    const QString pressed = portableShortcutForKeyPress(key, modifiers);
    if (pressed.isEmpty() ||
        snow_shot::storage::ScreenshotShortcutSettings::isReservedShortcut(pressed)) {
        return false;
    }

    const snow_shot::storage::ScreenshotShortcutSettings shortcuts;
    if (shortcutListContains(shortcuts.moveTool(), pressed)) {
        return m_context.actions.activateMoveTool();
    }
    if (!m_context.interaction.moveToolActive()) {
        return false;
    }
    if (shortcutListContains(shortcuts.moveCursorUp(), pressed)) {
        return m_context.actions.moveColorPickerCursor(0, -1);
    }
    if (shortcutListContains(shortcuts.moveCursorDown(), pressed)) {
        return m_context.actions.moveColorPickerCursor(0, 1);
    }
    if (shortcutListContains(shortcuts.moveCursorLeft(), pressed)) {
        return m_context.actions.moveColorPickerCursor(-1, 0);
    }
    if (shortcutListContains(shortcuts.moveCursorRight(), pressed)) {
        return m_context.actions.moveColorPickerCursor(1, 0);
    }
    return false;
}

bool ScreenshotOverlayInputHandler::handleDrawingShortcut(
    int key, Qt::KeyboardModifiers modifiers) {
    if (!(m_context.interaction.movingSelection() || m_context.interaction.editing()) ||
        recognitionTool(m_context.interaction.activeTool()) ||
        !m_context.actions.localShortcutInputAllowed()) {
        return false;
    }

    const QString pressed = portableShortcutForKeyPress(key, modifiers);
    if (pressed.isEmpty() ||
        snow_shot::storage::DrawingShortcutSettings::isReservedShortcut(pressed)) {
        return false;
    }

    const auto shortcutsByTool = snow_shot::storage::DrawingShortcutSettings().allShortcuts();
    for (auto toolIt = shortcutsByTool.cbegin(); toolIt != shortcutsByTool.cend(); ++toolIt) {
        for (const QString& shortcut : toolIt.value()) {
            if (shortcut.compare(pressed, Qt::CaseInsensitive) == 0) {
                return m_context.actions.activateDrawingShortcut(toolIt.key());
            }
        }
    }
    return false;
}

bool ScreenshotOverlayInputHandler::handleColorPickerKeyPress(int key,
                                                              Qt::KeyboardModifiers modifiers) {
    if (key == Qt::Key_C && modifiers == Qt::KeyboardModifiers() &&
        m_context.actions.copyColorPickerColorToClipboard()) {
        return true;
    }
    if (key == Qt::Key_Shift && modifiers == Qt::ShiftModifier &&
        m_context.actions.cycleColorPickerFormat()) {
        return true;
    }

    return false;
}

void ScreenshotOverlayInputHandler::requestIntelligentSelectionHitTest(
    const QPointF& virtualPosition) {
    m_context.actions.requestUiSelectorHitTest(physicalPositionForCanvasPoint(virtualPosition));
}

void ScreenshotOverlayInputHandler::setIntelligentSelectionIndex(int index) {
    if (!m_context.intelligentSelection.setIndex(index)) {
        m_context.intelligentSelection.reset();
        m_context.selection.clearSelection();
        return;
    }

    m_context.selection.setSelectionRect(m_context.intelligentSelection.currentSelection());
}

void ScreenshotOverlayInputHandler::confirmSelection() {
    const QRect selection = m_context.selection.pixelSelection();
    if (selection.width() < 1 || selection.height() < 1) {
        return;
    }

    m_context.interaction.confirmSelection();
    m_context.captureState.sessionState = ScreenshotSessionState::Editing;
    m_context.intelligentSelection.clearPress();
    m_context.actions.updateOverlayState();
    m_context.actions.showToolbar();
    m_context.actions.selectionConfirmed();
}

void ScreenshotOverlayInputHandler::handleUnhandledLeftDoubleClick() {
    if (!(m_context.interaction.movingSelection() || m_context.interaction.editing()) ||
        !m_context.selection.hasPixelSelection() ||
        !screenshotCompletionGestureTool(m_context.interaction.activeTool())) {
        return;
    }
    m_context.actions.executeConfiguredCompletionAction(
        snow_shot::storage::ScreenshotSettings().doubleClickAction());
}

void ScreenshotOverlayInputHandler::handleUnhandledMiddleClick() {
    if (!(m_context.interaction.movingSelection() || m_context.interaction.editing()) ||
        !m_context.selection.hasPixelSelection() ||
        !screenshotCompletionGestureTool(m_context.interaction.activeTool())) {
        return;
    }
    m_context.actions.executeConfiguredCompletionAction(
        snow_shot::storage::ScreenshotSettings().middleMouseButtonAction());
}

void ScreenshotOverlayInputHandler::updateGuideLines(ScreenshotOverlayWindow* overlay,
                                                     const QPointF& localPosition) const {
    m_context.actions.updateGuideLinesForOverlay(overlay, localPosition);
}

QPointF
ScreenshotOverlayInputHandler::virtualPositionForOverlay(const ScreenshotOverlayWindow* overlay,
                                                         const QPointF& localPosition) const {
    return m_context.geometry.canvasPositionForOverlayLocalPoint(m_context.displaySession, overlay,
                                                                 localPosition);
}

QPoint ScreenshotOverlayInputHandler::physicalPositionForCanvasPoint(const QPointF& point) const {
    return m_context.geometry.physicalPositionForCanvasPoint(m_context.displaySession, point);
}

ScreenshotSelectionDragMode
ScreenshotOverlayInputHandler::dragModeForVirtualPosition(const QPointF& virtualPosition,
                                                          bool borderOnly) const {
    return screenshotSelectionDragModeForPoint(
        m_context.selection.normalizedSelection(), virtualPosition, borderOnly,
        kSelectionEdgeTolerance, snow_shot::presentation::kScreenshotSelectionMinimumSize);
}

ScreenshotSelectionDragMode ScreenshotOverlayInputHandler::dragModeForPosition(
    const ScreenshotOverlayWindow* overlay, const QPointF& localPosition, bool borderOnly) const {
    return dragModeForVirtualPosition(virtualPositionForOverlay(overlay, localPosition),
                                      borderOnly);
}

QRectF ScreenshotOverlayInputHandler::selectionRectForDrag(ScreenshotSelectionDragMode dragMode,
                                                           const QPointF& position) const {
    return m_context.selection.selectionRectForDrag(
        dragMode, position, m_context.geometry.canvasBounds(),
        snow_shot::presentation::kScreenshotSelectionMinimumSize);
}
