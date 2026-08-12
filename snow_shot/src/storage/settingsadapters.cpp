#include "snow_shot/storage/settingsadapters.h"

#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/configurationstore.h"

#include "capturehistorypolicy_p.h"

#include <QJsonArray>

namespace snow_shot::storage {
namespace {
ConfigurationStore& cache() {
    auto& storage = ApplicationStorage::instance();
    if (!storage.isInitialized()) {
        static_cast<void>(storage.initialize());
    }
    return storage.configuration();
}

QStringList stringList(const QJsonValue& value) {
    QStringList result;
    for (const QJsonValue& item : value.toArray()) {
        result.push_back(item.toString());
    }
    return result;
}

QJsonArray stringArray(const QStringList& values) {
    QJsonArray result;
    for (const QString& value : values) {
        result.push_back(value);
    }
    return result;
}

QStringList shortcutValue(const QString& key) {
    return stringList(cache().value(key));
}

bool setShortcutValue(const QString& key, const QStringList& shortcuts) {
    return cache().setValue(key, stringArray(shortcuts));
}
} // namespace

QString InterfaceSettings::themeMode() const {
    return cache().value(QStringLiteral("interface/theme_mode")).toString();
}

bool InterfaceSettings::setThemeMode(const QString& mode) const {
    return cache().setValue(QStringLiteral("interface/theme_mode"), mode);
}

QString InterfaceSettings::language() const {
    return cache().value(QStringLiteral("interface/language")).toString();
}

bool InterfaceSettings::setLanguage(const QString& language) const {
    return cache().setValue(QStringLiteral("interface/language"), language);
}

bool InterfaceSettings::sidebarCollapsed() const {
    return cache().value(QStringLiteral("interface/sidebar_collapsed")).toBool();
}

bool InterfaceSettings::setSidebarCollapsed(bool collapsed) const {
    return cache().setValue(QStringLiteral("interface/sidebar_collapsed"), collapsed);
}

QStringList ShortcutSettings::screenshot() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot"));
}

bool ShortcutSettings::setScreenshot(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot"), shortcuts);
}

QStringList ShortcutSettings::screenshotDelay() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_delay"));
}

bool ShortcutSettings::setScreenshotDelay(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_delay"), shortcuts);
}

QStringList ShortcutSettings::screenshotFixed() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_fixed"));
}

bool ShortcutSettings::setScreenshotFixed(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_fixed"), shortcuts);
}

QStringList ShortcutSettings::screenshotOcr() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_ocr"));
}

bool ShortcutSettings::setScreenshotOcr(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_ocr"), shortcuts);
}

QStringList ShortcutSettings::screenshotCopy() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_copy"));
}

bool ShortcutSettings::setScreenshotCopy(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_copy"), shortcuts);
}

QStringList ShortcutSettings::screenshotFullScreen() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_full_screen"));
}

bool ShortcutSettings::setScreenshotFullScreen(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_full_screen"), shortcuts);
}

QStringList ShortcutSettings::screenshotFocusedWindow() const {
    return shortcutValue(QStringLiteral("global_shortcuts/screenshot_focused_window"));
}

bool ShortcutSettings::setScreenshotFocusedWindow(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/screenshot_focused_window"),
                            shortcuts);
}

QStringList ShortcutSettings::videoRecord() const {
    return shortcutValue(QStringLiteral("global_shortcuts/video_record"));
}

bool ShortcutSettings::setVideoRecord(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/video_record"), shortcuts);
}

QStringList ShortcutSettings::videoRecordCopy() const {
    return shortcutValue(QStringLiteral("global_shortcuts/video_record_copy"));
}

bool ShortcutSettings::setVideoRecordCopy(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/video_record_copy"), shortcuts);
}

QStringList ShortcutSettings::showOrHideMainWindow() const {
    return shortcutValue(QStringLiteral("global_shortcuts/show_or_hide_main_window"));
}

bool ShortcutSettings::setShowOrHideMainWindow(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/show_or_hide_main_window"), shortcuts);
}

QStringList ShortcutSettings::openCaptureHistory() const {
    return shortcutValue(QStringLiteral("global_shortcuts/open_capture_history"));
}

bool ShortcutSettings::setOpenCaptureHistory(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/open_capture_history"), shortcuts);
}

QStringList ShortcutSettings::openSettings() const {
    return shortcutValue(QStringLiteral("global_shortcuts/open_settings"));
}

bool ShortcutSettings::setOpenSettings(const QStringList& shortcuts) const {
    return setShortcutValue(QStringLiteral("global_shortcuts/open_settings"), shortcuts);
}

int ScreenshotSettings::delaySeconds() const {
    return cache().value(QStringLiteral("screenshot/delay_seconds")).toInt();
}

bool ScreenshotSettings::setDelaySeconds(int seconds) const {
    return cache().setValue(QStringLiteral("screenshot/delay_seconds"), seconds);
}

bool RecordingSettings::microphoneEnabled() const {
    return cache().value(QStringLiteral("video_recording/enable_microphone")).toBool();
}

bool RecordingSettings::setMicrophoneEnabled(bool enabled) const {
    return cache().setValue(QStringLiteral("video_recording/enable_microphone"), enabled);
}

bool RecordingSettings::systemAudioEnabled() const {
    return cache().value(QStringLiteral("video_recording/enable_system_audio")).toBool();
}

bool RecordingSettings::setSystemAudioEnabled(bool enabled) const {
    return cache().setValue(QStringLiteral("video_recording/enable_system_audio"), enabled);
}

QString ScreenshotToolbarSettings::arrowLineTool() const {
    return cache().value(QStringLiteral("screenshot_toolbar/arrow_line_tool")).toString();
}

bool ScreenshotToolbarSettings::setArrowLineTool(const QString& tool) const {
    return cache().setValue(QStringLiteral("screenshot_toolbar/arrow_line_tool"), tool);
}

QString ScreenshotToolbarSettings::highlightTool() const {
    return cache().value(QStringLiteral("screenshot_toolbar/highlight_tool")).toString();
}

bool ScreenshotToolbarSettings::setHighlightTool(const QString& tool) const {
    return cache().setValue(QStringLiteral("screenshot_toolbar/highlight_tool"), tool);
}

QString ScreenshotToolbarSettings::tableQrTool() const {
    return cache().value(QStringLiteral("screenshot_toolbar/table_qr_tool")).toString();
}

bool ScreenshotToolbarSettings::setTableQrTool(const QString& tool) const {
    return cache().setValue(QStringLiteral("screenshot_toolbar/table_qr_tool"), tool);
}

CaptureHistoryPolicy HistorySettings::policy() const {
    return captureHistoryPolicyFromConfiguration(cache());
}

std::shared_future<StorageResult>
HistorySettings::setPolicy(const CaptureHistoryPolicy& policy) const {
    auto& storage = ApplicationStorage::instance();
    if (!storage.isInitialized()) {
        static_cast<void>(storage.initialize());
    }
    return storage.requestCaptureHistoryPolicyAsync(policy);
}

std::shared_future<StorageResult> HistorySettings::setEnabled(bool enabled) const {
    CaptureHistoryPolicy next = policy();
    next.enabled = enabled;
    return setPolicy(next);
}

std::shared_future<StorageResult> HistorySettings::setRetentionDays(int days) const {
    CaptureHistoryPolicy next = policy();
    next.retentionDays = days;
    return setPolicy(next);
}

std::shared_future<StorageResult> HistorySettings::setMaxEntries(int entries) const {
    CaptureHistoryPolicy next = policy();
    next.maxEntries = entries;
    return setPolicy(next);
}

std::shared_future<StorageResult> HistorySettings::setMaxDiskMiB(int mebibytes) const {
    CaptureHistoryPolicy next = policy();
    next.maxDiskMiB = mebibytes;
    return setPolicy(next);
}
} // namespace snow_shot::storage
