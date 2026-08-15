#include "snow_shot/presentation/screenshotshortcuthints.h"

#include <QCoreApplication>
#include <QStringList>

#include <initializer_list>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        qFatal("%s", message);
    }
}

QStringList hintLines(ScreenshotActiveTool tool,
                      std::initializer_list<SnowCanvasTool> disabled = {}) {
    ScreenshotShortcutHintContext context;
    context.activeTool = tool;
    context.captureMode = ScreenshotCaptureMode::Editing;
    for (const SnowCanvasTool disabledTool : disabled) {
        context.quickSelectionDisabledTools.insert(disabledTool);
    }
    return screenshotShortcutHintLines(context);
}

void toolMatrixMatchesRequestedVisibility() {
    const QStringList transformHints{
        QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
        QStringLiteral("Maintain aspect ratio: Shift"),
        QStringLiteral("Fixed-angle rotation: Shift"),
        QStringLiteral("Scale from center: Alt"),
        QStringLiteral("Auto-align: Ctrl"),
        QStringLiteral("Delete selected elements: Delete"),
    };
    require(hintLines(ScreenshotActiveTool::Select) == transformHints,
            "selection tool hint matrix changed");
    require(hintLines(ScreenshotActiveTool::Shape) == transformHints,
            "shape tool hint matrix changed");
    require(hintLines(ScreenshotActiveTool::Arrow) == transformHints,
            "arrow tool hint matrix changed");
    require(hintLines(ScreenshotActiveTool::Line) == transformHints,
            "line tool hint matrix changed");
    require(hintLines(ScreenshotActiveTool::RectangleHighlight) == transformHints,
            "rectangle-highlighter hint matrix changed");
    require(hintLines(ScreenshotActiveTool::RectangleFilter) == transformHints,
            "rectangle-filter hint matrix changed");

    QStringList penTransformHints{
        QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
        QStringLiteral("Draw straight line: Shift")};
    penTransformHints.append(transformHints.mid(1));
    require(hintLines(ScreenshotActiveTool::FreeDraw) == penTransformHints,
            "free-draw hint matrix changed");
    require(hintLines(ScreenshotActiveTool::PenFilter) == penTransformHints,
            "pen-filter hint matrix changed");
    require(hintLines(ScreenshotActiveTool::PenHighlight) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                            QStringLiteral("Delete selected elements: Delete")},
            "pen-highlighter hint matrix changed");

    const QStringList textTransformHints{
        QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
        QStringLiteral("Fixed-angle rotation: Shift"),
        QStringLiteral("Scale from center: Alt"),
        QStringLiteral("Auto-align: Ctrl"),
        QStringLiteral("Delete selected elements: Delete"),
    };
    require(hintLines(ScreenshotActiveTool::Text) == textTransformHints,
            "text hint matrix changed");
    require(hintLines(ScreenshotActiveTool::SerialNumber) == textTransformHints,
            "serial-number hint matrix changed");

    require(hintLines(ScreenshotActiveTool::Shape, {SnowCanvasTool::Shape}) ==
                QStringList{
                    QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                    QStringLiteral("Maintain aspect ratio: Shift"),
                    QStringLiteral("Scale from center: Alt"),
                    QStringLiteral("Auto-align: Ctrl"),
                },
            "shape quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::Arrow, {SnowCanvasTool::Arrow}) ==
                QStringList{
                    QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                    QStringLiteral("Fixed-angle rotation: Shift"),
                    QStringLiteral("Auto-align: Ctrl"),
                },
            "arrow quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::Line, {SnowCanvasTool::Line}) ==
                QStringList{
                    QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                    QStringLiteral("Fixed-angle rotation: Shift"),
                    QStringLiteral("Auto-align: Ctrl"),
                },
            "line quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::RectangleHighlight,
                      {SnowCanvasTool::RectangleHighlight}) ==
                QStringList{
                    QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                    QStringLiteral("Maintain aspect ratio: Shift"),
                    QStringLiteral("Scale from center: Alt"),
                    QStringLiteral("Auto-align: Ctrl"),
                },
            "rectangle-highlighter quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::RectangleFilter,
                      {SnowCanvasTool::RectangleFilter}) ==
                QStringList{
                    QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                    QStringLiteral("Maintain aspect ratio: Shift"),
                    QStringLiteral("Scale from center: Alt"),
                    QStringLiteral("Auto-align: Ctrl"),
                },
            "rectangle-filter quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::FreeDraw, {SnowCanvasTool::FreeDraw}) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                            QStringLiteral("Draw straight line: Shift")},
            "free-draw quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::PenFilter, {SnowCanvasTool::PenFilter}) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys"),
                            QStringLiteral("Draw straight line: Shift")},
            "pen-filter quick-selection suppression changed");
    require(hintLines(ScreenshotActiveTool::PenHighlight, {SnowCanvasTool::PenHighlight}) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys")},
            "pen-highlighter delete hint should be suppressed");
    require(hintLines(ScreenshotActiveTool::Text, {SnowCanvasTool::Text}) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys")},
            "text hints should be suppressed when quick selection is disabled");
    require(hintLines(ScreenshotActiveTool::SerialNumber, {SnowCanvasTool::SerialNumber}) ==
                QStringList{QStringLiteral("Move cursor: W, S, A, D, Arrow keys")},
            "serial-number hints should be suppressed when quick selection is disabled");

    require(hintLines(ScreenshotActiveTool::Eraser).isEmpty() &&
                hintLines(ScreenshotActiveTool::Ocr).isEmpty() &&
                hintLines(ScreenshotActiveTool::Table).isEmpty() &&
                hintLines(ScreenshotActiveTool::Qr).isEmpty() &&
                hintLines(ScreenshotActiveTool::Move).isEmpty() &&
                hintLines(ScreenshotActiveTool::Spotlight).isEmpty() &&
                hintLines(ScreenshotActiveTool::Watermark).isEmpty(),
            "tools without requested shortcuts must not expose hint rows");
}

void scrollingHintsUseMouseWheelLabels() {
    ScreenshotShortcutHintContext context;
    context.activeTool = ScreenshotActiveTool::Move;
    context.captureMode = ScreenshotCaptureMode::ScrollingCapture;
    require(screenshotShortcutHintModeForContext(context) == ScreenshotShortcutHintMode::Scrolling,
            "scrolling capture should use the scrolling hint mode");
    require(screenshotShortcutHintLines(context) ==
                QStringList{
                    QStringLiteral("Vertical scroll: mouse wheel"),
                    QStringLiteral("Horizontal scroll: Shift + mouse wheel"),
                },
            "scrolling capture hint labels changed");
}

void selectionStageContextsRetainShortcutHints() {
    ScreenshotShortcutHintContext context;
    context.activeTool = ScreenshotActiveTool::Move;

    context.captureMode = ScreenshotCaptureMode::IntelligentSelecting;
    require(screenshotShortcutHintSelectionModeForContext(context) ==
                    ScreenshotShortcutHintMode::SmartSelection &&
                screenshotShortcutHintModeForContext(context) ==
                    ScreenshotShortcutHintMode::SmartSelection &&
                screenshotShortcutHintLines(context) ==
                    screenshotShortcutHintLines(ScreenshotShortcutHintMode::SmartSelection),
            "intelligent selection must retain its shortcut hints through the context resolver");

    context.captureMode = ScreenshotCaptureMode::ManualSelecting;
    require(screenshotShortcutHintSelectionModeForContext(context) ==
                    ScreenshotShortcutHintMode::Selection &&
                screenshotShortcutHintModeForContext(context) ==
                    ScreenshotShortcutHintMode::Selection &&
                screenshotShortcutHintLines(context) ==
                    screenshotShortcutHintLines(ScreenshotShortcutHintMode::Selection),
            "manual selection must retain its shortcut hints through the context resolver");

    context.captureMode = ScreenshotCaptureMode::MovingSelection;
    require(screenshotShortcutHintSelectionModeForContext(context) ==
                    ScreenshotShortcutHintMode::Selection &&
                screenshotShortcutHintModeForContext(context) ==
                    ScreenshotShortcutHintMode::Selection &&
                screenshotShortcutHintLines(context) ==
                    screenshotShortcutHintLines(ScreenshotShortcutHintMode::Selection),
            "the Move tool must retain selection shortcut hints after confirmation");

    context.activeTool = ScreenshotActiveTool::Select;
    require(screenshotShortcutHintSelectionModeForContext(context) ==
                    ScreenshotShortcutHintMode::Hidden &&
                screenshotShortcutHintModeForContext(context) ==
                    ScreenshotShortcutHintMode::Hidden &&
                screenshotShortcutHintLines(context).isEmpty(),
            "moving-selection hints must remain exclusive to the Move tool");
}

void emptyContextsUseHiddenMode() {
    ScreenshotShortcutHintContext context;
    context.activeTool = ScreenshotActiveTool::Select;
    context.captureMode = ScreenshotCaptureMode::Editing;
    require(screenshotShortcutHintModeForContext(context) == ScreenshotShortcutHintMode::Tool,
            "a populated drawing-tool context should use tool hint mode");

    context.activeTool = ScreenshotActiveTool::PenHighlight;
    context.quickSelectionDisabledTools.insert(SnowCanvasTool::PenHighlight);
    require(screenshotShortcutHintModeForContext(context) == ScreenshotShortcutHintMode::Tool,
            "cursor movement should keep a conditionally empty tool context visible");

    context.activeTool = ScreenshotActiveTool::Select;
    context.captureMode = ScreenshotCaptureMode::Inactive;
    context.quickSelectionDisabledTools.clear();
    require(screenshotShortcutHintModeForContext(context) == ScreenshotShortcutHintMode::Hidden,
            "non-editing drawing contexts should not leave stale tool hints visible");
}

void hintAreaHidesForSelectionOverlapOrCursorHover() {
    const QRectF hintArea(16.0, 300.0, 240.0, 180.0);
    require(!screenshotShortcutHintAreaIsObscured(
                hintArea, QRectF(300.0, 100.0, 200.0, 150.0), QPointF(500.0, 500.0)),
            "a separate selection and cursor must leave shortcut hints visible");
    require(screenshotShortcutHintAreaIsObscured(
                hintArea, QRectF(200.0, 250.0, 100.0, 100.0), QPointF(500.0, 500.0)),
            "a selection overlapping the shortcut hint area must hide it");
    require(screenshotShortcutHintAreaIsObscured(
                hintArea, QRectF(), QPointF(100.0, 350.0)),
            "a cursor over the shortcut hint area must hide it");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    toolMatrixMatchesRequestedVisibility();
    scrollingHintsUseMouseWheelLabels();
    selectionStageContextsRetainShortcutHints();
    emptyContextsUseHiddenMode();
    hintAreaHidesForSelectionOverlapOrCursorHover();
    return 0;
}
