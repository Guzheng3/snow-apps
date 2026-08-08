#include "snow_shot/presentation/settings/settingsruntimebindings.h"

#include "snow_shot/presentation/languagemanager.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/storage/configurationschema.h"

#include <QJsonArray>

namespace snow_shot::presentation::settings {
namespace {
QString themeModeValue(styles::ThemeMode mode) {
    switch (mode) {
    case styles::ThemeMode::Light:
        return QStringLiteral("light");
    case styles::ThemeMode::Dark:
        return QStringLiteral("dark");
    case styles::ThemeMode::FollowSystem:
    default:
        return QStringLiteral("system");
    }
}

styles::ThemeMode themeModeForValue(const QVariant& value) {
    const QString key = value.toString();
    if (key == QStringLiteral("light")) {
        return styles::ThemeMode::Light;
    }
    if (key == QStringLiteral("dark")) {
        return styles::ThemeMode::Dark;
    }
    return styles::ThemeMode::FollowSystem;
}

QStringList stringListDefault(const QString& key) {
    QStringList result;
    const QJsonArray values = storage::ConfigurationSchema::defaultValue(key).toArray();
    result.reserve(values.size());
    for (const QJsonValue& value : values) {
        result.push_back(value.toString());
    }
    return result;
}

storage::CaptureHistoryPolicy defaultHistoryPolicy() {
    storage::CaptureHistoryPolicy policy;
    policy.enabled = storage::ConfigurationSchema::defaultValue(
                         QStringLiteral("capture_history/enabled"))
                         .toBool();
    policy.retentionDays = storage::ConfigurationSchema::defaultValue(
                               QStringLiteral("capture_history/retention_days"))
                               .toInt();
    policy.maxEntries = storage::ConfigurationSchema::defaultValue(
                            QStringLiteral("capture_history/max_entries"))
                            .toInt();
    policy.maxDiskMiB = storage::ConfigurationSchema::defaultValue(
                            QStringLiteral("capture_history/max_disk_mib"))
                            .toInt();
    return policy;
}
} // namespace

BuiltInSettingsRuntimeBindings::BuiltInSettingsRuntimeBindings(
    GlobalShortcutManager& shortcutManager, QObject* parent)
    : SettingsRuntimeBindings(parent), m_shortcutManager(shortcutManager) {
    auto& themeManager = styles::ThemeManager::instance();
    connect(&themeManager, &styles::ThemeManager::themeModeChanged, this,
            [this](styles::ThemeMode) { emit synchronized(); });

    auto& languageManager = LanguageManager::instance();
    connect(&languageManager, &LanguageManager::languageChanged, this,
            [this](const QString&, const QLocale&) { emit synchronized(); });
    connect(&languageManager, &LanguageManager::languageChangeFailed, this,
            [this](const QString&) { emit synchronized(); });

    connect(&m_shortcutManager, &GlobalShortcutManager::stateChanged, this,
            [this](GlobalShortcutAction action,
                   const GlobalShortcutRegistrationState& state) {
                emit shortcutStateChanged(action, state);
                emit synchronized();
            });

    auto& applicationStorage = storage::ApplicationStorage::instance();
    if (!applicationStorage.isInitialized()) {
        static_cast<void>(applicationStorage.initialize());
    }
    connect(&applicationStorage, &storage::ApplicationStorage::storageStatusChanged, this,
            [this](const storage::StorageStatus&) { emit synchronized(); });
    connect(&applicationStorage,
            &storage::ApplicationStorage::captureHistoryClearFinished, this,
            [this](bool, const QString&) { emit synchronized(); });
    connect(&applicationStorage, &storage::ApplicationStorage::smartSelectionChanged, this,
            [this](bool) { emit synchronized(); });
}

QVariant BuiltInSettingsRuntimeBindings::selectValue(SettingsSelectBinding binding) const {
    switch (binding) {
    case SettingsSelectBinding::Theme:
        return themeModeValue(styles::ThemeManager::instance().themeMode());
    case SettingsSelectBinding::Language:
        return LanguageManager::instance().languagePreference();
    }
    return {};
}

QVector<SettingsRuntimeOption>
BuiltInSettingsRuntimeBindings::dynamicSelectOptions(SettingsSelectBinding binding) const {
    QVector<SettingsRuntimeOption> result;
    if (binding != SettingsSelectBinding::Language) {
        return result;
    }
    const QList<LanguageCatalog> languages = LanguageManager::instance().availableLanguages();
    result.reserve(languages.size());
    for (const LanguageCatalog& language : languages) {
        result.push_back({language.localeName, language.nativeName});
    }
    return result;
}

bool BuiltInSettingsRuntimeBindings::applySelectValue(SettingsSelectBinding binding,
                                                      const QVariant& value) {
    switch (binding) {
    case SettingsSelectBinding::Theme: {
        const auto requested = themeModeForValue(value);
        styles::ThemeManager::instance().setThemeMode(requested);
        return styles::ThemeManager::instance().themeMode() == requested;
    }
    case SettingsSelectBinding::Language:
        return LanguageManager::instance().setLanguage(value.toString());
    }
    return false;
}

bool BuiltInSettingsRuntimeBindings::switchValue(SettingsSwitchBinding binding) const {
    switch (binding) {
    case SettingsSwitchBinding::HistoryEnabled:
        return storage::ApplicationStorage::instance().captureHistoryPolicy().enabled;
    case SettingsSwitchBinding::SmartSelection:
        return storage::ApplicationStorage::instance().smartSelectionEnabled();
    }
    return false;
}

bool BuiltInSettingsRuntimeBindings::applySwitchValue(SettingsSwitchBinding binding, bool value) {
    if (binding == SettingsSwitchBinding::SmartSelection) {
        return storage::ApplicationStorage::instance().requestSmartSelection(value);
    }

    auto policy = storage::ApplicationStorage::instance().captureHistoryPolicy();
    switch (binding) {
    case SettingsSwitchBinding::HistoryEnabled:
        policy.enabled = value;
        break;
    case SettingsSwitchBinding::SmartSelection:
        return false;
    }
    return storage::ApplicationStorage::instance().requestCaptureHistoryPolicy(policy);
}

int BuiltInSettingsRuntimeBindings::integerValue(SettingsIntegerBinding binding) const {
    const storage::CaptureHistoryPolicy policy =
        storage::ApplicationStorage::instance().captureHistoryPolicy();
    switch (binding) {
    case SettingsIntegerBinding::HistoryRetentionDays:
        return policy.retentionDays;
    case SettingsIntegerBinding::HistoryMaxEntries:
        return policy.maxEntries;
    case SettingsIntegerBinding::HistoryMaxDiskMiB:
        return policy.maxDiskMiB;
    }
    return 0;
}

bool BuiltInSettingsRuntimeBindings::applyIntegerValue(SettingsIntegerBinding binding, int value) {
    auto policy = storage::ApplicationStorage::instance().captureHistoryPolicy();
    switch (binding) {
    case SettingsIntegerBinding::HistoryRetentionDays:
        policy.retentionDays = value;
        break;
    case SettingsIntegerBinding::HistoryMaxEntries:
        policy.maxEntries = value;
        break;
    case SettingsIntegerBinding::HistoryMaxDiskMiB:
        policy.maxDiskMiB = value;
        break;
    }
    return storage::ApplicationStorage::instance().requestCaptureHistoryPolicy(policy);
}

GlobalShortcutRegistrationState
BuiltInSettingsRuntimeBindings::shortcutState(GlobalShortcutAction action) const {
    return m_shortcutManager.state(action);
}

GlobalShortcutValidationResult
BuiltInSettingsRuntimeBindings::validateShortcut(const QString& shortcut) const {
    return m_shortcutManager.validateShortcut(shortcut);
}

bool BuiltInSettingsRuntimeBindings::applyShortcuts(GlobalShortcutAction action,
                                                   const QStringList& shortcuts) {
    m_shortcutManager.setShortcuts(action, shortcuts);
    return m_shortcutManager.state(action).shortcuts == shortcuts;
}

SettingsActionState
BuiltInSettingsRuntimeBindings::actionState(SettingsActionBinding binding) const {
    const storage::StorageStatus status = storage::ApplicationStorage::instance().status();
    switch (binding) {
    case SettingsActionBinding::ClearCaptureHistory:
        return {
            status.writeAvailable && !status.historyClearing &&
                (status.historyUsage.entryCount > 0 || status.historyUsage.totalBytes > 0),
            status.historyClearing,
        };
    }
    return {};
}

bool BuiltInSettingsRuntimeBindings::triggerAction(SettingsActionBinding binding) {
    switch (binding) {
    case SettingsActionBinding::ClearCaptureHistory:
        return storage::ApplicationStorage::instance().requestCaptureHistoryClear();
    }
    return false;
}

storage::StorageStatus BuiltInSettingsRuntimeBindings::storageStatus() const {
    return storage::ApplicationStorage::instance().status();
}

bool BuiltInSettingsRuntimeBindings::resetSection(SettingsSectionReset reset) {
    switch (reset) {
    case SettingsSectionReset::ScreenshotShortcuts:
        return applyShortcuts(GlobalShortcutAction::Screenshot,
                              stringListDefault(QStringLiteral("global_shortcuts/screenshot")));
    case SettingsSectionReset::OpenSettingsShortcuts:
        return applyShortcuts(GlobalShortcutAction::OpenSettings,
                              stringListDefault(QStringLiteral("global_shortcuts/open_settings")));
    case SettingsSectionReset::GeneralSettings: {
        const bool themeAccepted = applySelectValue(
            SettingsSelectBinding::Theme,
            storage::ConfigurationSchema::defaultValue(QStringLiteral("interface/theme_mode")));
        const bool languageAccepted = applySelectValue(
            SettingsSelectBinding::Language,
            storage::ConfigurationSchema::defaultValue(QStringLiteral("interface/language")));
        return themeAccepted && languageAccepted;
    }
    case SettingsSectionReset::HistoryPolicy:
        return storage::ApplicationStorage::instance().requestCaptureHistoryPolicy(
            defaultHistoryPolicy());
    case SettingsSectionReset::ScreenshotSettings:
        return storage::ApplicationStorage::instance().requestSmartSelection(
            storage::ConfigurationSchema::defaultValue(
                QStringLiteral("screenshot_selection/smart_selection"))
                .toBool());
    case SettingsSectionReset::None:
        return true;
    }
    return false;
}

} // namespace snow_shot::presentation::settings
