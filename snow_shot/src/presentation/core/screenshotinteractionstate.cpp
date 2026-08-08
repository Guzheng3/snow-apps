#include "snow_shot/presentation/screenshotinteractionstate.h"

void ScreenshotInteractionState::reset() {
    m_activeTool = ScreenshotActiveTool::Move;
    m_mode = ScreenshotCaptureMode::Inactive;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::beginCapture() {
    m_activeTool = ScreenshotActiveTool::Move;
    m_mode = ScreenshotCaptureMode::ManualSelecting;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::enterOverlayVisible(bool selectorReady) {
    m_activeTool = ScreenshotActiveTool::Move;
    m_mode = selectorReady ? ScreenshotCaptureMode::IntelligentSelecting
                           : ScreenshotCaptureMode::ManualSelecting;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::setMoveTool(bool hasSelection, bool selectorReady) {
    m_activeTool = ScreenshotActiveTool::Move;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
    if (hasSelection) {
        m_mode = ScreenshotCaptureMode::MovingSelection;
        return;
    }
    m_mode = selectorReady ? ScreenshotCaptureMode::IntelligentSelecting
                           : ScreenshotCaptureMode::ManualSelecting;
}

void ScreenshotInteractionState::setCanvasTool(ScreenshotActiveTool tool) {
    m_activeTool = tool;
    m_mode = ScreenshotCaptureMode::Editing;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::setOcrTool() {
    setCanvasTool(ScreenshotActiveTool::Ocr);
}

void ScreenshotInteractionState::setTableTool() {
    setCanvasTool(ScreenshotActiveTool::Table);
}

void ScreenshotInteractionState::setQrTool() {
    setCanvasTool(ScreenshotActiveTool::Qr);
}

void ScreenshotInteractionState::confirmSelection() {
    m_mode = ScreenshotCaptureMode::MovingSelection;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::applySelectionParams() {
    confirmSelection();
}

void ScreenshotInteractionState::enterScrollingCapture() {
    m_activeTool = ScreenshotActiveTool::Move;
    m_mode = ScreenshotCaptureMode::ScrollingCapture;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::returnToSelectionMode(bool selectorReady) {
    m_activeTool = ScreenshotActiveTool::Move;
    m_mode = selectorReady ? ScreenshotCaptureMode::IntelligentSelecting
                           : ScreenshotCaptureMode::ManualSelecting;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::enterManualSelectionDrag() {
    m_mode = ScreenshotCaptureMode::ManualSelecting;
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = true;
}

void ScreenshotInteractionState::enterManualSelectionDragFromIntelligent() {
    enterManualSelectionDrag();
}

bool ScreenshotInteractionState::enterMovingSelectionDrag(ScreenshotSelectionDragMode dragMode) {
    if (dragMode == ScreenshotSelectionDragMode::None) {
        return false;
    }

    m_mode = ScreenshotCaptureMode::MovingSelection;
    m_dragMode = dragMode;
    m_dragging = true;
    return true;
}

void ScreenshotInteractionState::finishDrag() {
    m_dragMode = ScreenshotSelectionDragMode::None;
    m_dragging = false;
}

void ScreenshotInteractionState::cancelDrag() {
    finishDrag();
}

ScreenshotActiveTool ScreenshotInteractionState::activeTool() const {
    return m_activeTool;
}

ScreenshotCaptureMode ScreenshotInteractionState::mode() const {
    return m_mode;
}

ScreenshotSelectionDragMode ScreenshotInteractionState::dragMode() const {
    return m_dragMode;
}

bool ScreenshotInteractionState::dragging() const {
    return m_dragging;
}

bool ScreenshotInteractionState::inactive() const {
    return m_mode == ScreenshotCaptureMode::Inactive;
}

bool ScreenshotInteractionState::moveToolActive() const {
    return m_activeTool == ScreenshotActiveTool::Move;
}

bool ScreenshotInteractionState::intelligentSelecting() const {
    return m_mode == ScreenshotCaptureMode::IntelligentSelecting;
}

bool ScreenshotInteractionState::manualSelecting() const {
    return m_mode == ScreenshotCaptureMode::ManualSelecting;
}

bool ScreenshotInteractionState::movingSelection() const {
    return m_mode == ScreenshotCaptureMode::MovingSelection;
}

bool ScreenshotInteractionState::editing() const {
    return m_mode == ScreenshotCaptureMode::Editing;
}

bool ScreenshotInteractionState::scrollingCapture() const {
    return m_mode == ScreenshotCaptureMode::ScrollingCapture;
}

bool ScreenshotInteractionState::selecting() const {
    return intelligentSelecting() || manualSelecting();
}

bool ScreenshotInteractionState::selectionToolbarMode() const {
    return intelligentSelecting() || manualSelecting() || movingSelection() || editing();
}

bool ScreenshotInteractionState::canResizeSelection() const {
    return movingSelection() || editing();
}
