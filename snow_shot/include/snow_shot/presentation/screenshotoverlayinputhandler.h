#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTOVERLAYINPUTHANDLER_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTOVERLAYINPUTHANDLER_H

#include "snow_shot/presentation/screenshotselectiongeometry.h"

#include <QPoint>
#include <QPointF>
#include <Qt>

#include <functional>

class ScreenshotDisplaySession;
class ScreenshotGeometryMapper;
class ScreenshotInteractionState;
class ScreenshotIntelligentSelectionModel;
class ScreenshotOverlayWindow;
class ScreenshotSelectionModel;
struct ScreenshotCaptureState;

struct ScreenshotOverlayInputActions {
    std::function<bool(const QPoint& physicalPoint)> returnToIntelligentSelection =
        [](const QPoint&) { return false; };
    std::function<void(const QPoint& physicalPoint)> requestUiSelectorHitTest = [](const QPoint&) {
    };
    std::function<void()> pauseIntelligentSelection = []() {};

    std::function<void(ScreenshotOverlayWindow* overlay, ScreenshotSelectionDragMode dragMode)>
        setOverlayCursor = [](ScreenshotOverlayWindow*, ScreenshotSelectionDragMode) {};
    std::function<void()> hideMainToolbar = []() {};
    std::function<void()> updateOverlayState = []() {};
    std::function<void()> showToolbar = []() {};
    std::function<void()> showSelectionToolbar = []() {};
    std::function<void()> cancelCapture = []() {};
    std::function<bool(int delta)> stepStrokeWidth = [](int) { return false; };
    std::function<bool(int delta)> stepSelectionOpacity = [](int) { return false; };
    std::function<bool(int delta)> stepSpotlightOpacity = [](int) { return false; };
    std::function<bool(int delta)> stepFilterIntensity = [](int) { return false; };
    std::function<bool(int delta)> stepPenFilterStrokeWidth = [](int) { return false; };
    std::function<bool(int delta)> stepWatermarkFontSize = [](int) { return false; };
    std::function<void()> copySelectionToClipboard = []() {};
    std::function<void(const QString& action)> executeConfiguredCompletionAction =
        [](const QString&) {};
    std::function<bool()> drawingShortcutInputAllowed = []() { return true; };
    std::function<bool(const QString& toolId)> activateDrawingShortcut =
        [](const QString&) { return false; };
    std::function<bool()> navigateHistoryPrevious = []() { return false; };
    std::function<bool()> navigateHistoryNext = []() { return false; };
    std::function<bool()> returnToCurrentScreenshot = []() { return false; };

    std::function<void(ScreenshotOverlayWindow* overlay, const QPointF& localPosition)>
        updateColorPickerForOverlay = [](ScreenshotOverlayWindow*, const QPointF&) {};
    std::function<void(ScreenshotOverlayWindow* overlay, const QPointF& localPosition)>
        updateGuideLinesForOverlay = [](ScreenshotOverlayWindow*, const QPointF&) {};
    std::function<void(const QPointF& virtualPosition)> updateColorPickerForSelectionDrag =
        [](const QPointF&) {};
    std::function<bool()> copyColorPickerColorToClipboard = []() { return false; };
    std::function<bool()> cycleColorPickerFormat = []() { return false; };
    std::function<bool(int dx, int dy)> moveColorPickerCursor = [](int, int) { return false; };

    // Called after a valid selection transitions the interaction into editing.
    // This is intentionally separate from showToolbar so callers can schedule
    // a post-selection command (for example, OCR or pinning) without coupling
    // the input handler to a concrete controller.
    std::function<void()> selectionConfirmed = []() {};
};

struct ScreenshotOverlayInputHandlerContext {
    ScreenshotCaptureState& captureState;
    ScreenshotInteractionState& interaction;
    ScreenshotSelectionModel& selection;
    ScreenshotIntelligentSelectionModel& intelligentSelection;
    const ScreenshotGeometryMapper& geometry;
    const ScreenshotDisplaySession& displaySession;
    ScreenshotOverlayInputActions actions;
};

class ScreenshotOverlayInputHandler final {
  public:
    explicit ScreenshotOverlayInputHandler(ScreenshotOverlayInputHandlerContext context);

    void handleMousePress(ScreenshotOverlayWindow* overlay, const QPointF& localPosition);
    [[nodiscard]] bool shouldHandleMouseEvent(const ScreenshotOverlayWindow* overlay,
                                              const QPointF& localPosition,
                                              bool leftButtonActive) const;
    void handleMouseMove(ScreenshotOverlayWindow* overlay, const QPointF& localPosition);
    void handleMouseRelease(ScreenshotOverlayWindow* overlay, const QPointF& localPosition);
    [[nodiscard]] bool handleRightClick(ScreenshotOverlayWindow* overlay,
                                        const QPointF& localPosition);
    void handleUnhandledLeftDoubleClick();
    void handleUnhandledMiddleClick();
    [[nodiscard]] bool handleWheel(ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
                                   const QPoint& angleDelta, const QPoint& pixelDelta);
    [[nodiscard]] bool handleKeyPress(int key, Qt::KeyboardModifiers modifiers);

  private:
    void handleMovingSelectionPress(ScreenshotOverlayWindow* overlay,
                                    const QPointF& virtualPosition);
    void handleIntelligentSelectionPress(const QPointF& virtualPosition);
    void handleManualSelectionPress(ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
                                    const QPointF& virtualPosition);
    void handleIntelligentSelectionMove(ScreenshotOverlayWindow* overlay,
                                        const QPointF& localPosition,
                                        const QPointF& virtualPosition);
    void handleHoverMove(ScreenshotOverlayWindow* overlay, const QPointF& localPosition);
    void updateGuideLines(ScreenshotOverlayWindow* overlay, const QPointF& localPosition) const;
    void handleMovingSelectionDragMove(const QPointF& virtualPosition);
    void handleManualSelectionDragMove(ScreenshotOverlayWindow* overlay,
                                       const QPointF& localPosition,
                                       const QPointF& virtualPosition);
    void handleIntelligentSelectionRelease(const QPointF& virtualPosition);
    void handleMovingSelectionRelease(ScreenshotOverlayWindow* overlay,
                                      const QPointF& localPosition, const QPointF& virtualPosition);
    void handleManualSelectionRelease(ScreenshotOverlayWindow* overlay,
                                      const QPointF& localPosition, const QPointF& virtualPosition);
    [[nodiscard]] bool handleColorPickerKeyPress(int key, Qt::KeyboardModifiers modifiers);
    [[nodiscard]] bool handleDrawingShortcut(int key, Qt::KeyboardModifiers modifiers);
    void requestIntelligentSelectionHitTest(const QPointF& virtualPosition);
    void setIntelligentSelectionIndex(int index);
  public:
    // Confirms the current model selection and invokes selectionConfirmed.
    // This is also used by non-interactive quick actions that select a whole
    // monitor or a focused window after the capture frame arrives.
    void confirmSelection();

  private:
    [[nodiscard]] QPointF virtualPositionForOverlay(const ScreenshotOverlayWindow* overlay,
                                                    const QPointF& localPosition) const;
    [[nodiscard]] QPoint physicalPositionForCanvasPoint(const QPointF& point) const;
    [[nodiscard]] ScreenshotSelectionDragMode
    dragModeForVirtualPosition(const QPointF& virtualPosition, bool borderOnly) const;
    [[nodiscard]] ScreenshotSelectionDragMode
    dragModeForPosition(const ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
                        bool borderOnly) const;
    [[nodiscard]] QRectF selectionRectForDrag(ScreenshotSelectionDragMode dragMode,
                                              const QPointF& position) const;

    ScreenshotOverlayInputHandlerContext m_context;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTOVERLAYINPUTHANDLER_H
