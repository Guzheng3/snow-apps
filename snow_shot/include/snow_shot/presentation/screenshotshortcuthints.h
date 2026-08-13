#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H

#include <QCoreApplication>
#include <QStringList>

enum class ScreenshotShortcutHintMode {
    Hidden,
    SmartSelection,
    Selection,
};

[[nodiscard]] inline ScreenshotShortcutHintMode screenshotShortcutHintModeForState(
    bool intelligentSelecting, bool manualSelecting, bool movingSelection, bool moveToolActive) {
    if (intelligentSelecting) {
        return ScreenshotShortcutHintMode::SmartSelection;
    }
    if (manualSelecting || (movingSelection && moveToolActive)) {
        return ScreenshotShortcutHintMode::Selection;
    }
    return ScreenshotShortcutHintMode::Hidden;
}

[[nodiscard]] inline QStringList
screenshotShortcutHintLines(ScreenshotShortcutHintMode mode) {
    if (mode == ScreenshotShortcutHintMode::Hidden) {
        return {};
    }

    QStringList lines;
    if (mode == ScreenshotShortcutHintMode::SmartSelection) {
        lines.push_back(QCoreApplication::translate(
            "ScreenshotShortcutHintsWidget", "Switch element level: mouse wheel"));
    }
    lines.push_back(
        QCoreApplication::translate("ScreenshotShortcutHintsWidget", "Copy color: C"));
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Switch color format: Shift"));
    lines.push_back(QCoreApplication::translate(
        "ScreenshotShortcutHintsWidget", "Switch screenshot history: [ , ] [ . ]"));
    return lines;
}

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTSHORTCUTHINTS_H
