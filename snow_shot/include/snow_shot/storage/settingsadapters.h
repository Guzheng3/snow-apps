#ifndef SNOW_SHOT_STORAGE_SETTINGSADAPTERS_H
#define SNOW_SHOT_STORAGE_SETTINGSADAPTERS_H

#include "snow_shot/storage/capturehistorytypes.h"

#include <QString>
#include <QStringList>

#include <future>

namespace snow_shot::storage {
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
    [[nodiscard]] QStringList openSettings() const;
    bool setOpenSettings(const QStringList& shortcuts) const;
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
