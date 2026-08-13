#ifndef SNOW_SHOT_STORAGE_SETTINGSADAPTERS_H
#define SNOW_SHOT_STORAGE_SETTINGSADAPTERS_H

#include "snow_shot/storage/capturehistorytypes.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

#include <future>

namespace snow_shot::storage {
struct ScreenshotToolbarLayout {
    QVector<QStringList> positions;
    QStringList hidden;

    friend bool operator==(const ScreenshotToolbarLayout& first,
                           const ScreenshotToolbarLayout& second) {
        return first.positions == second.positions && first.hidden == second.hidden;
    }
    friend bool operator!=(const ScreenshotToolbarLayout& first,
                           const ScreenshotToolbarLayout& second) {
        return !(first == second);
    }
};

[[nodiscard]] QColor colorFromRgbaString(const QString& value);
[[nodiscard]] QString colorToRgbaString(const QColor& color);

class InterfaceSettings final {
  public:
    [[nodiscard]] QString themeMode() const;
    bool setThemeMode(const QString& mode) const;
    [[nodiscard]] QString language() const;
    bool setLanguage(const QString& language) const;
    [[nodiscard]] bool sidebarCollapsed() const;
    bool setSidebarCollapsed(bool collapsed) const;
};

class ShortcutSettings final {
  public:
    [[nodiscard]] QStringList screenshot() const;
    bool setScreenshot(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotDelay() const;
    bool setScreenshotDelay(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotFixed() const;
    bool setScreenshotFixed(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotOcr() const;
    bool setScreenshotOcr(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotCopy() const;
    bool setScreenshotCopy(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotFullScreen() const;
    bool setScreenshotFullScreen(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList screenshotFocusedWindow() const;
    bool setScreenshotFocusedWindow(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList videoRecord() const;
    bool setVideoRecord(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList videoRecordCopy() const;
    bool setVideoRecordCopy(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList showOrHideMainWindow() const;
    bool setShowOrHideMainWindow(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList openCaptureHistory() const;
    bool setOpenCaptureHistory(const QStringList& shortcuts) const;
    [[nodiscard]] QStringList openSettings() const;
    bool setOpenSettings(const QStringList& shortcuts) const;
};

class ScreenshotSettings final {
  public:
    [[nodiscard]] int delaySeconds() const;
    bool setDelaySeconds(int seconds) const;
};

struct ScreenshotTranslationConfiguration {
    QString sourceLanguage;
    QString targetLanguage;
    QString modelId;

    friend bool operator==(const ScreenshotTranslationConfiguration& first,
                           const ScreenshotTranslationConfiguration& second) = default;
};

class ScreenshotTranslationSettings final {
  public:
    [[nodiscard]] ScreenshotTranslationConfiguration configuration() const;
    bool setConfiguration(const ScreenshotTranslationConfiguration& configuration) const;
};

class ScreenshotUiSettings final {
  public:
    [[nodiscard]] QString toolbarSize() const;
    bool setToolbarSize(const QString& size) const;
    [[nodiscard]] bool selectionTransitionAnimationEnabled() const;
    bool setSelectionTransitionAnimationEnabled(bool enabled) const;
    [[nodiscard]] QString colorPickerDisplayMode() const;
    bool setColorPickerDisplayMode(const QString& mode) const;
    [[nodiscard]] QColor selectionMaskColor() const;
    bool setSelectionMaskColor(const QColor& color) const;
    [[nodiscard]] int shortcutHintOpacity() const;
    bool setShortcutHintOpacity(int opacity) const;
    [[nodiscard]] QColor cursorGuideLineColor() const;
    bool setCursorGuideLineColor(const QColor& color) const;
    [[nodiscard]] QColor monitorCenterGuideLineColor() const;
    bool setMonitorCenterGuideLineColor(const QColor& color) const;
    [[nodiscard]] QColor colorPickerCenterGuideLineColor() const;
    bool setColorPickerCenterGuideLineColor(const QColor& color) const;
};

class RecordingSettings final {
  public:
    [[nodiscard]] bool microphoneEnabled() const;
    bool setMicrophoneEnabled(bool enabled) const;
    [[nodiscard]] bool systemAudioEnabled() const;
    bool setSystemAudioEnabled(bool enabled) const;
};

class ScreenshotToolbarSettings final {
  public:
    [[nodiscard]] QString arrowLineTool() const;
    bool setArrowLineTool(const QString& tool) const;
    [[nodiscard]] QString highlightTool() const;
    bool setHighlightTool(const QString& tool) const;
    [[nodiscard]] QString tableQrTool() const;
    bool setTableQrTool(const QString& tool) const;
    [[nodiscard]] ScreenshotToolbarLayout layout() const;
    bool setLayout(const ScreenshotToolbarLayout& layout) const;
};

class PinToScreenSettings final {
  public:
    [[nodiscard]] QColor borderColor() const;
    bool setBorderColor(const QColor& color) const;
};

class TraySettings final {
  public:
    [[nodiscard]] bool enabled() const;
    bool setEnabled(bool enabled) const;
    [[nodiscard]] QString icon() const;
    bool setIcon(const QString& icon) const;
    [[nodiscard]] QString customIcon() const;
    bool setCustomIcon(const QString& path) const;
};

class HistorySettings final {
  public:
    [[nodiscard]] CaptureHistoryPolicy policy() const;
    [[nodiscard]] std::shared_future<StorageResult>
    setPolicy(const CaptureHistoryPolicy& policy) const;
    [[nodiscard]] std::shared_future<StorageResult> setEnabled(bool enabled) const;
    [[nodiscard]] std::shared_future<StorageResult> setRetentionDays(int days) const;
    [[nodiscard]] std::shared_future<StorageResult> setMaxEntries(int entries) const;
    [[nodiscard]] std::shared_future<StorageResult> setMaxDiskMiB(int mebibytes) const;
};
} // namespace snow_shot::storage

#endif // SNOW_SHOT_STORAGE_SETTINGSADAPTERS_H
