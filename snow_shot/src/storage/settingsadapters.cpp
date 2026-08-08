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
    return stringList(cache().value(QStringLiteral("global_shortcuts/screenshot")));
}

bool ShortcutSettings::setScreenshot(const QStringList& shortcuts) const {
    return cache().setValue(QStringLiteral("global_shortcuts/screenshot"), stringArray(shortcuts));
}

QStringList ShortcutSettings::openSettings() const {
    return stringList(cache().value(QStringLiteral("global_shortcuts/open_settings")));
}

bool ShortcutSettings::setOpenSettings(const QStringList& shortcuts) const {
    return cache().setValue(QStringLiteral("global_shortcuts/open_settings"),
                            stringArray(shortcuts));
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
