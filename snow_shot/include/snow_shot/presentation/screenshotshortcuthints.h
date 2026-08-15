#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H

#include "snow_draw_engine_qt/snow_canvas_types.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"

#include <QCoreApplication>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QStringList>

enum class ScreenshotShortcutHintMode {
    Hidden,
    SmartSelection,
    Selection,
    Tool,
    Scrolling,
};

// The hint list is a property of the active canvas workflow, rather than of the
// overlay widget itself. Keeping this context value-type makes the visibility
// matrix easy to exercise without constructing the screenshot UI.
struct ScreenshotShortcutHintContext {
    ScreenshotActiveTool activeTool = ScreenshotActiveTool::Move;
    ScreenshotCaptureMode captureMode = ScreenshotCaptureMode::Inactive;
    QSet<SnowCanvasTool> quickSelectionDisabledTools;
};

[[nodiscard]] inline bool screenshotShortcutHintAreaIsObscured(
    const QRectF& hintArea, const QRectF& selectionArea, const QPointF& cursorPosition) {
    if (!hintArea.isValid() || hintArea.isEmpty()) {
        return false;
    }
    const bool selectionOverlaps = selectionArea.isValid() && !selectionArea.isEmpty() &&
                                   hintArea.intersects(selectionArea);
    return selectionOverlaps || hintArea.contains(cursorPosition);
}

[[nodiscard]] inline ScreenshotShortcutHintMode screenshotShortcutHintSelectionModeForContext(
    const ScreenshotShortcutHintContext& context) {
    if (context.captureMode == ScreenshotCaptureMode::IntelligentSelecting) {
        return ScreenshotShortcutHintMode::SmartSelection;
    }
    if (context.captureMode == ScreenshotCaptureMode::ManualSelecting ||
        (context.captureMode == ScreenshotCaptureMode::MovingSelection &&
         context.activeTool == ScreenshotActiveTool::Move)) {
        return ScreenshotShortcutHintMode::Selection;
    }
    return ScreenshotShortcutHintMode::Hidden;
}

[[nodiscard]] inline QStringList
screenshotShortcutHintLines(ScreenshotShortcutHintMode mode) {
    if (mode == ScreenshotShortcutHintMode::Hidden ||
        mode == ScreenshotShortcutHintMode::Tool) {
        return {};
    }
    if (mode == ScreenshotShortcutHintMode::Scrolling) {
        return {
            QCoreApplication::translate("ScreenshotShortcutHintsWidget",
                                        "Vertical scroll: mouse wheel"),
            QCoreApplication::translate("ScreenshotShortcutHintsWidget",
                                        "Horizontal scroll: Shift + mouse wheel"),
        };
    }

    QStringList lines;
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Move cursor: W, S, A, D, Arrow keys"));
    if (mode == ScreenshotShortcutHintMode::SmartSelection) {
        lines.push_back(QCoreApplication::translate(
            "ScreenshotShortcutHintsWidget", "Switch element level: mouse wheel"));
        lines.push_back(QCoreApplication::translate(
            "ScreenshotShortcutHintsWidget", "Select Window/Window Sub-element: Tab"));
    } else {
        lines.push_back(QCoreApplication::translate(
            "ScreenshotShortcutHintsWidget", "Move Entire Selection: Space"));
        lines.push_back(QCoreApplication::translate(
            "ScreenshotShortcutHintsWidget",
            "Keep Selection Width and Height Consistent: Shift"));
    }
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Select Previously Selected Area: R"));
    lines.push_back(
        QCoreApplication::translate("ScreenshotShortcutHintsWidget", "Copy Color: C"));
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Switch Color Format: Shift"));
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Switch Screenshot History: [ , ] [ . ]"));
    return lines;
}

[[nodiscard]] inline bool screenshotShortcutHintToolIsQuickSelectionDisabled(
    const ScreenshotShortcutHintContext& context, SnowCanvasTool tool) {
    return context.quickSelectionDisabledTools.contains(tool);
}

[[nodiscard]] inline QString screenshotShortcutHintLine(const char* source) {
    return QCoreApplication::translate("ScreenshotShortcutHintsWidget", source);
}

[[nodiscard]] inline QStringList
screenshotShortcutHintLines(const ScreenshotShortcutHintContext& context) {
    if (context.captureMode == ScreenshotCaptureMode::ScrollingCapture) {
        return {
            screenshotShortcutHintLine("Vertical scroll: mouse wheel"),
            screenshotShortcutHintLine("Horizontal scroll: Shift + mouse wheel"),
        };
    }

    // Selection-stage hints are independent of the currently selected canvas
    // tool. Keep these stages mapped to the legacy modes so intelligent and
    // manual selection retain their context-specific shortcuts.
    const ScreenshotShortcutHintMode selectionMode =
        screenshotShortcutHintSelectionModeForContext(context);
    if (selectionMode != ScreenshotShortcutHintMode::Hidden) {
        return screenshotShortcutHintLines(selectionMode);
    }

    // Hints are intentionally limited to the canvas editing tools after the
    // selection stages above. Recognition workflows have their own transient
    // UI and should not leave stale drawing instructions over the capture.
    if (context.captureMode != ScreenshotCaptureMode::Editing) {
        return {};
    }

    const auto disabled = [&context](SnowCanvasTool tool) {
        return screenshotShortcutHintToolIsQuickSelectionDisabled(context, tool);
    };
    const auto append = [](QStringList& lines, const char* source, bool enabled = true) {
        if (enabled) {
            lines.push_back(screenshotShortcutHintLine(source));
        }
    };

    QStringList lines;
  if (context.activeTool != ScreenshotActiveTool::Eraser &&
      context.activeTool != ScreenshotActiveTool::Ocr &&
      context.activeTool != ScreenshotActiveTool::Table &&
      context.activeTool != ScreenshotActiveTool::Qr &&
      context.activeTool != ScreenshotActiveTool::Move &&
      context.activeTool != ScreenshotActiveTool::Spotlight &&
      context.activeTool != ScreenshotActiveTool::Watermark) {
    append(lines, "Move cursor: W, S, A, D, Arrow keys");
  }
    switch (context.activeTool) {
    case ScreenshotActiveTool::Select:
        append(lines, "Maintain aspect ratio: Shift");
        append(lines, "Fixed-angle rotation: Shift");
        append(lines, "Scale from center: Alt");
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete");
        break;
    case ScreenshotActiveTool::Shape:
        append(lines, "Maintain aspect ratio: Shift");
        append(lines, "Fixed-angle rotation: Shift", !disabled(SnowCanvasTool::Shape));
        append(lines, "Scale from center: Alt");
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::Shape));
        break;
    case ScreenshotActiveTool::Arrow:
        append(lines, "Maintain aspect ratio: Shift", !disabled(SnowCanvasTool::Arrow));
        append(lines, "Fixed-angle rotation: Shift");
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::Arrow));
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::Arrow));
        break;
    case ScreenshotActiveTool::Line:
        append(lines, "Maintain aspect ratio: Shift", !disabled(SnowCanvasTool::Line));
        append(lines, "Fixed-angle rotation: Shift");
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::Line));
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::Line));
        break;
    case ScreenshotActiveTool::FreeDraw:
        append(lines, "Draw straight line: Shift");
        append(lines, "Maintain aspect ratio: Shift", !disabled(SnowCanvasTool::FreeDraw));
        append(lines, "Fixed-angle rotation: Shift", !disabled(SnowCanvasTool::FreeDraw));
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::FreeDraw));
        append(lines, "Auto-align: Ctrl", !disabled(SnowCanvasTool::FreeDraw));
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::FreeDraw));
        break;
    case ScreenshotActiveTool::RectangleHighlight:
        append(lines, "Maintain aspect ratio: Shift");
        append(lines, "Fixed-angle rotation: Shift",
               !disabled(SnowCanvasTool::RectangleHighlight));
        append(lines, "Scale from center: Alt");
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete",
               !disabled(SnowCanvasTool::RectangleHighlight));
        break;
    case ScreenshotActiveTool::PenHighlight:
        append(lines, "Delete selected elements: Delete",
               !disabled(SnowCanvasTool::PenHighlight));
        break;
    case ScreenshotActiveTool::Text:
        append(lines, "Fixed-angle rotation: Shift", !disabled(SnowCanvasTool::Text));
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::Text));
        append(lines, "Auto-align: Ctrl", !disabled(SnowCanvasTool::Text));
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::Text));
        break;
    case ScreenshotActiveTool::SerialNumber:
        append(lines, "Fixed-angle rotation: Shift", !disabled(SnowCanvasTool::SerialNumber));
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::SerialNumber));
        append(lines, "Auto-align: Ctrl", !disabled(SnowCanvasTool::SerialNumber));
        append(lines, "Delete selected elements: Delete",
               !disabled(SnowCanvasTool::SerialNumber));
        break;
    case ScreenshotActiveTool::PenFilter:
        append(lines, "Draw straight line: Shift");
        append(lines, "Maintain aspect ratio: Shift", !disabled(SnowCanvasTool::PenFilter));
        append(lines, "Fixed-angle rotation: Shift", !disabled(SnowCanvasTool::PenFilter));
        append(lines, "Scale from center: Alt", !disabled(SnowCanvasTool::PenFilter));
        append(lines, "Auto-align: Ctrl", !disabled(SnowCanvasTool::PenFilter));
        append(lines, "Delete selected elements: Delete", !disabled(SnowCanvasTool::PenFilter));
        break;
    case ScreenshotActiveTool::RectangleFilter:
        append(lines, "Maintain aspect ratio: Shift");
        append(lines, "Fixed-angle rotation: Shift",
               !disabled(SnowCanvasTool::RectangleFilter));
        append(lines, "Scale from center: Alt");
        append(lines, "Auto-align: Ctrl");
        append(lines, "Delete selected elements: Delete",
               !disabled(SnowCanvasTool::RectangleFilter));
        break;
    case ScreenshotActiveTool::Eraser:
    case ScreenshotActiveTool::Ocr:
    case ScreenshotActiveTool::Table:
    case ScreenshotActiveTool::Qr:
    case ScreenshotActiveTool::Move:
    case ScreenshotActiveTool::Spotlight:
    case ScreenshotActiveTool::Watermark:
        break;
    }
    return lines;
}

[[nodiscard]] inline ScreenshotShortcutHintMode
screenshotShortcutHintModeForContext(const ScreenshotShortcutHintContext& context) {
    if (context.captureMode == ScreenshotCaptureMode::ScrollingCapture) {
        return ScreenshotShortcutHintMode::Scrolling;
    }

    const ScreenshotShortcutHintMode selectionMode =
        screenshotShortcutHintSelectionModeForContext(context);
    if (selectionMode != ScreenshotShortcutHintMode::Hidden) {
        return selectionMode;
    }

    return screenshotShortcutHintLines(context).isEmpty() ? ScreenshotShortcutHintMode::Hidden
                                                          : ScreenshotShortcutHintMode::Tool;
}

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H
