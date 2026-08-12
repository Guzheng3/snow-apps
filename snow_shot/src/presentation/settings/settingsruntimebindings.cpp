#include "snow_shot/presentation/settings/settingsruntimebindings.h"
#include "snow_shot/presentation/settings/applicationpriority.h"
#include "snow_shot/presentation/settings/textrecognitionacceleration.h"

#include "snow_shot/presentation/languagemanager.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/storage/configurationschema.h"
#include "snow_shot/storage/settingsadapters.h"

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
    case SettingsSelectBinding::ApplicationPriority: {
        auto& storage = storage::ApplicationStorage::instance();
        if (!storage.isInitialized()) {
            static_cast<void>(storage.initialize());
        }
        return storage.configuration().value(QStringLiteral("system/application_priority"))
            .toString();
    }
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
    case SettingsSelectBinding::ApplicationPriority: {
        const auto requested = applicationPriorityForValue(value.toString());
        if (!requested.has_value()) {
            return false;
        }
        auto& storage = storage::ApplicationStorage::instance();
        if (!storage.isInitialized()) {
            static_cast<void>(storage.initialize());
        }
        const auto previous = applicationPriorityForValue(
            storage.configuration().value(QStringLiteral("system/application_priority"))
                .toString());
        if (!applyApplicationPriority(*requested)) {
            return false;
        }
        const bool persisted = storage.configuration().setValue(
            QStringLiteral("system/application_priority"), applicationPriorityValue(*requested));
        if (persisted) {
            emit synchronized();
        } else if (previous.has_value()) {
            static_cast<void>(applyApplicationPriority(*previous));
        }
        return persisted;
    }
    }
    return false;
}

bool BuiltInSettingsRuntimeBindings::switchValue(SettingsSwitchBinding binding) const {
    switch (binding) {
    case SettingsSwitchBinding::HistoryEnabled:
        return storage::ApplicationStorage::instance().captureHistoryPolicy().enabled;
    case SettingsSwitchBinding::SmartSelection:
        return storage::ApplicationStorage::instance().smartSelectionEnabled();
    case SettingsSwitchBinding::DirectMlAcceleration:
        return directMlTextRecognitionSupported() &&
               storage::ApplicationStorage::instance()
                   .configuration()
                   .value(QStringLiteral("text_recognition/direct_ml_acceleration"))
                   .toBool();
    }
    return false;
}

bool BuiltInSettingsRuntimeBindings::switchEnabled(SettingsSwitchBinding binding) const {
    return binding != SettingsSwitchBinding::DirectMlAcceleration ||
           directMlTextRecognitionSupported();
}

bool BuiltInSettingsRuntimeBindings::applySwitchValue(SettingsSwitchBinding binding, bool value) {
    if (binding == SettingsSwitchBinding::SmartSelection) {
        return storage::ApplicationStorage::instance().requestSmartSelection(value);
    }
    if (binding == SettingsSwitchBinding::DirectMlAcceleration) {
        if (!directMlTextRecognitionSupported()) {
            return false;
        }
        auto& storage = storage::ApplicationStorage::instance();
        const bool accepted = storage.configuration().setValue(
            QStringLiteral("text_recognition/direct_ml_acceleration"), value);
        if (accepted) {
            emit synchronized();
        }
        return accepted;
    }

    auto policy = storage::ApplicationStorage::instance().captureHistoryPolicy();
    switch (binding) {
    case SettingsSwitchBinding::HistoryEnabled:
        policy.enabled = value;
        break;
    case SettingsSwitchBinding::SmartSelection:
        return false;
    case SettingsSwitchBinding::DirectMlAcceleration:
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
    case SettingsIntegerBinding::ScreenshotDelaySeconds:
        return storage::ScreenshotSettings().delaySeconds();
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
    case SettingsIntegerBinding::ScreenshotDelaySeconds: {
        const auto* schema = storage::ConfigurationSchema::entry(
            QStringLiteral("screenshot/delay_seconds"));
        if (schema == nullptr || !schema->integerRange.has_value() ||
            value < schema->integerRange->minimum || value > schema->integerRange->maximum) {
            return false;
        }
        const bool accepted = storage::ScreenshotSettings().setDelaySeconds(value);
        if (accepted) {
            emit synchronized();
        }
        return accepted;
    }
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
    case SettingsSectionReset::ScreenshotShortcuts: {
        bool accepted = true;
        const auto resetShortcut = [this, &accepted](GlobalShortcutAction action,
                                                      const QString& key) {
            accepted = applyShortcuts(action, stringListDefault(key)) && accepted;
        };
        resetShortcut(GlobalShortcutAction::Screenshot,
                      QStringLiteral("global_shortcuts/screenshot"));
        resetShortcut(GlobalShortcutAction::ScreenshotDelay,
                      QStringLiteral("global_shortcuts/screenshot_delay"));
        resetShortcut(GlobalShortcutAction::ScreenshotFixed,
                      QStringLiteral("global_shortcuts/screenshot_fixed"));
        resetShortcut(GlobalShortcutAction::ScreenshotOcr,
                      QStringLiteral("global_shortcuts/screenshot_ocr"));
        resetShortcut(GlobalShortcutAction::ScreenshotCopy,
                      QStringLiteral("global_shortcuts/screenshot_copy"));
        resetShortcut(GlobalShortcutAction::ScreenshotFullScreen,
                      QStringLiteral("global_shortcuts/screenshot_full_screen"));
        resetShortcut(GlobalShortcutAction::ScreenshotFocusedWindow,
                      QStringLiteral("global_shortcuts/screenshot_focused_window"));
        accepted = applyIntegerValue(
                       SettingsIntegerBinding::ScreenshotDelaySeconds,
                       storage::ConfigurationSchema::defaultValue(
                           QStringLiteral("screenshot/delay_seconds"))
                           .toInt()) &&
                   accepted;
        return accepted;
    }
    case SettingsSectionReset::OpenSettingsShortcuts: {
        bool accepted = true;
        const auto resetShortcut = [this, &accepted](GlobalShortcutAction action,
                                                      const QString& key) {
            accepted = applyShortcuts(action, stringListDefault(key)) && accepted;
        };
        resetShortcut(GlobalShortcutAction::ShowOrHideMainWindow,
                      QStringLiteral("global_shortcuts/show_or_hide_main_window"));
        resetShortcut(GlobalShortcutAction::OpenCaptureHistory,
                      QStringLiteral("global_shortcuts/open_capture_history"));
        resetShortcut(GlobalShortcutAction::OpenSettings,
                      QStringLiteral("global_shortcuts/open_settings"));
        return accepted;
    }
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
    case SettingsSectionReset::SystemSettings: {
        auto& storage = storage::ApplicationStorage::instance();
        if (!storage.isInitialized()) {
            static_cast<void>(storage.initialize());
        }
        const auto previous = applicationPriorityForValue(
            storage.configuration().value(QStringLiteral("system/application_priority"))
                .toString());
        const auto priority = applicationPriorityForValue(
            storage::ConfigurationSchema::defaultValue(
                QStringLiteral("system/application_priority"))
                .toString());
        const bool accepted =
            priority.has_value() && applyApplicationPriority(*priority) &&
            storage.configuration().setValue(
                QStringLiteral("system/application_priority"), applicationPriorityValue(*priority));
        if (accepted) {
            emit synchronized();
        } else if (previous.has_value()) {
            static_cast<void>(applyApplicationPriority(*previous));
        }
        return accepted;
    }
    case SettingsSectionReset::TextRecognition: {
        auto& storage = storage::ApplicationStorage::instance();
        const bool accepted = storage.configuration().setValue(
            QStringLiteral("text_recognition/direct_ml_acceleration"),
            storage::ConfigurationSchema::defaultValue(
                QStringLiteral("text_recognition/direct_ml_acceleration")));
        if (accepted) {
            emit synchronized();
        }
        return accepted;
    }
    case SettingsSectionReset::None:
        return true;
    }
    return false;
}

} // namespace snow_shot::presentation::settings
