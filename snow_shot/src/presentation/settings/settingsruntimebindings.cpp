#include "snow_shot/presentation/settings/settingsruntimebindings.h"
#include "snow_shot/presentation/settings/applicationpriority.h"
#include "snow_shot/presentation/settings/textrecognitionacceleration.h"
#include "snow_shot/platform/windows/autostartregistration.h"

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

bool applyAutoStartAtBoot(bool enabled) {
    using snow_shot::platform::windows::AutoStartRegistration;
    auto& configuration = storage::ApplicationStorage::instance().configuration();
    const bool previousConfiguredValue =
        configuration.value(QStringLiteral("system/auto_start_at_boot")).toBool();
    const auto previous = AutoStartRegistration::snapshot();
    if (!previous.valid || !AutoStartRegistration::setEnabled(enabled)) {
        return false;
    }
    if (storage::SystemSettings().setAutoStartAtBoot(enabled) &&
        configuration.flushNow().success) {
        return true;
    }
    static_cast<void>(AutoStartRegistration::restore(previous));
    if (configuration.value(QStringLiteral("system/auto_start_at_boot")).toBool() !=
        previousConfiguredValue) {
        static_cast<void>(configuration.setValue(
            QStringLiteral("system/auto_start_at_boot"), previousConfiguredValue));
        static_cast<void>(configuration.flushNow());
    }
    return false;
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
    connect(&applicationStorage.configuration(), &storage::ConfigurationStore::valueChanged,
            this, [this](const QString&, const QJsonValue&) { emit synchronized(); });
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
    case SettingsSelectBinding::ScreenshotToolbarSize:
        return storage::ScreenshotUiSettings().toolbarSize();
    case SettingsSelectBinding::ColorPickerDisplayMode:
        return storage::ScreenshotUiSettings().colorPickerDisplayMode();
    case SettingsSelectBinding::ScreenshotOcrAction:
        return storage::ScreenshotSettings().autoExecuteAfterTextRecognition();
    case SettingsSelectBinding::ScreenshotDoubleClickAction:
        return storage::ScreenshotSettings().doubleClickAction();
    case SettingsSelectBinding::ScreenshotMiddleClickAction:
        return storage::ScreenshotSettings().middleMouseButtonAction();
    case SettingsSelectBinding::PinMouseWheelZoomMode:
        return storage::PinToScreenSettings().mouseWheelZoomMode();
    case SettingsSelectBinding::VideoClarity:
        return storage::RecordingSettings().videoClarity();
    case SettingsSelectBinding::VideoFrameRate:
        return storage::RecordingSettings().frameRate();
    case SettingsSelectBinding::AnimatedImageClarity:
        return storage::RecordingSettings().animatedImageClarity();
    case SettingsSelectBinding::AnimatedImageFrameRate:
        return storage::RecordingSettings().animatedImageFrameRate();
    case SettingsSelectBinding::AnimatedImageFormat:
        return storage::RecordingSettings().animatedImageFormat();
    case SettingsSelectBinding::VideoEncoder:
        return storage::RecordingSettings().encoder();
    case SettingsSelectBinding::VideoEncodingPreset:
        return storage::RecordingSettings().encodingPreset();
    case SettingsSelectBinding::TrayLeftClickAction:
        return storage::TraySettings().leftClickAction();
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
    case SettingsSelectBinding::ScreenshotToolbarSize:
        return storage::ScreenshotUiSettings().setToolbarSize(value.toString());
    case SettingsSelectBinding::ColorPickerDisplayMode:
        return storage::ScreenshotUiSettings().setColorPickerDisplayMode(value.toString());
    case SettingsSelectBinding::ScreenshotOcrAction:
        return storage::ScreenshotSettings().setAutoExecuteAfterTextRecognition(value.toString());
    case SettingsSelectBinding::ScreenshotDoubleClickAction:
        return storage::ScreenshotSettings().setDoubleClickAction(value.toString());
    case SettingsSelectBinding::ScreenshotMiddleClickAction:
        return storage::ScreenshotSettings().setMiddleMouseButtonAction(value.toString());
    case SettingsSelectBinding::PinMouseWheelZoomMode:
        return storage::PinToScreenSettings().setMouseWheelZoomMode(value.toString());
    case SettingsSelectBinding::VideoClarity:
        return storage::RecordingSettings().setVideoClarity(value.toString());
    case SettingsSelectBinding::VideoFrameRate:
        return storage::RecordingSettings().setFrameRate(value.toInt());
    case SettingsSelectBinding::AnimatedImageClarity:
        return storage::RecordingSettings().setAnimatedImageClarity(value.toString());
    case SettingsSelectBinding::AnimatedImageFrameRate:
        return storage::RecordingSettings().setAnimatedImageFrameRate(value.toInt());
    case SettingsSelectBinding::AnimatedImageFormat:
        return storage::RecordingSettings().setAnimatedImageFormat(value.toString());
    case SettingsSelectBinding::VideoEncoder:
        return storage::RecordingSettings().setEncoder(value.toString());
    case SettingsSelectBinding::VideoEncodingPreset:
        return storage::RecordingSettings().setEncodingPreset(value.toString());
    case SettingsSelectBinding::TrayLeftClickAction:
        return storage::TraySettings().setLeftClickAction(value.toString());
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
    case SettingsSwitchBinding::SelectionTransitionAnimation:
        return storage::ScreenshotUiSettings().selectionTransitionAnimationEnabled();
    case SettingsSwitchBinding::TrayEnabled:
        return storage::TraySettings().enabled();
    case SettingsSwitchBinding::ScreenshotAutoSaveAfterCopy:
        return storage::ScreenshotSettings().autoSaveAfterCopy();
    case SettingsSwitchBinding::ScreenshotCopyImageFileToClipboard:
        return storage::ScreenshotSettings().copyImageFileToClipboard();
    case SettingsSwitchBinding::PinAutomaticTextRecognition:
        return storage::PinToScreenSettings().automaticTextRecognition();
    case SettingsSwitchBinding::PinAutoResizeWindow:
        return storage::PinToScreenSettings().autoResizeWindow();
    case SettingsSwitchBinding::VideoHideToolbarInRecording:
        return storage::RecordingSettings().hideToolbarInRecording();
    case SettingsSwitchBinding::DisableHotkeysOnFocusedFullscreen:
        return storage::GlobalShortcutSettings().disableOnFocusedFullscreenWindow();
    case SettingsSwitchBinding::AutoStartAtBoot:
        return storage::SystemSettings().autoStartAtBoot();
    }
    return false;
}

bool BuiltInSettingsRuntimeBindings::switchEnabled(SettingsSwitchBinding binding) const {
    if (binding == SettingsSwitchBinding::AutoStartAtBoot) {
        return snow_shot::platform::windows::AutoStartRegistration::isSupported();
    }
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

    if (binding == SettingsSwitchBinding::SelectionTransitionAnimation) {
        return storage::ScreenshotUiSettings().setSelectionTransitionAnimationEnabled(value);
    }
    if (binding == SettingsSwitchBinding::TrayEnabled) {
        return storage::TraySettings().setEnabled(value);
    }
    if (binding == SettingsSwitchBinding::ScreenshotAutoSaveAfterCopy) {
        return storage::ScreenshotSettings().setAutoSaveAfterCopy(value);
    }
    if (binding == SettingsSwitchBinding::ScreenshotCopyImageFileToClipboard) {
        return storage::ScreenshotSettings().setCopyImageFileToClipboard(value);
    }
    if (binding == SettingsSwitchBinding::PinAutomaticTextRecognition) {
        return storage::PinToScreenSettings().setAutomaticTextRecognition(value);
    }
    if (binding == SettingsSwitchBinding::PinAutoResizeWindow) {
        return storage::PinToScreenSettings().setAutoResizeWindow(value);
    }
    if (binding == SettingsSwitchBinding::VideoHideToolbarInRecording) {
        return storage::RecordingSettings().setHideToolbarInRecording(value);
    }
    if (binding == SettingsSwitchBinding::DisableHotkeysOnFocusedFullscreen) {
        return storage::GlobalShortcutSettings().setDisableOnFocusedFullscreenWindow(value);
    }
    if (binding == SettingsSwitchBinding::AutoStartAtBoot) {
        return applyAutoStartAtBoot(value);
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
    case SettingsSwitchBinding::SelectionTransitionAnimation:
    case SettingsSwitchBinding::TrayEnabled:
    case SettingsSwitchBinding::ScreenshotAutoSaveAfterCopy:
    case SettingsSwitchBinding::ScreenshotCopyImageFileToClipboard:
    case SettingsSwitchBinding::PinAutomaticTextRecognition:
    case SettingsSwitchBinding::PinAutoResizeWindow:
    case SettingsSwitchBinding::VideoHideToolbarInRecording:
    case SettingsSwitchBinding::DisableHotkeysOnFocusedFullscreen:
    case SettingsSwitchBinding::AutoStartAtBoot:
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

QVariantList BuiltInSettingsRuntimeBindings::multiSelectValue(
    SettingsMultiSelectBinding binding) const {
    QVariantList values;
    if (binding == SettingsMultiSelectBinding::DrawingQuickSelectionDisabledTools) {
        for (const QString& value : storage::DrawingSettings().quickSelectionDisabledTools()) {
            values.push_back(value);
        }
    }
    return values;
}

bool BuiltInSettingsRuntimeBindings::applyMultiSelectValue(
    SettingsMultiSelectBinding binding, const QVariantList& value) {
    if (binding != SettingsMultiSelectBinding::DrawingQuickSelectionDisabledTools) {
        return false;
    }
    QStringList tools;
    tools.reserve(value.size());
    for (const QVariant& item : value) {
        tools.push_back(item.toString());
    }
    return storage::DrawingSettings().setQuickSelectionDisabledTools(tools);
}

int BuiltInSettingsRuntimeBindings::sliderValue(SettingsSliderBinding binding) const {
    switch (binding) {
    case SettingsSliderBinding::ShortcutHintOpacity:
        return storage::ScreenshotUiSettings().shortcutHintOpacity();
    }
    return 0;
}

bool BuiltInSettingsRuntimeBindings::applySliderValue(SettingsSliderBinding binding, int value) {
    switch (binding) {
    case SettingsSliderBinding::ShortcutHintOpacity:
        return storage::ScreenshotUiSettings().setShortcutHintOpacity(value);
    }
    return false;
}

QColor BuiltInSettingsRuntimeBindings::colorValue(SettingsColorBinding binding) const {
    const storage::ScreenshotUiSettings screenshot;
    switch (binding) {
    case SettingsColorBinding::SelectionMaskColor:
        return screenshot.selectionMaskColor();
    case SettingsColorBinding::CursorGuideLineColor:
        return screenshot.cursorGuideLineColor();
    case SettingsColorBinding::MonitorCenterGuideLineColor:
        return screenshot.monitorCenterGuideLineColor();
    case SettingsColorBinding::ColorPickerCenterGuideLineColor:
        return screenshot.colorPickerCenterGuideLineColor();
    case SettingsColorBinding::PinBorderColor:
        return storage::PinToScreenSettings().borderColor();
    }
    return {};
}

bool BuiltInSettingsRuntimeBindings::applyColorValue(SettingsColorBinding binding,
                                                      const QColor& value) {
    const storage::ScreenshotUiSettings screenshot;
    switch (binding) {
    case SettingsColorBinding::SelectionMaskColor:
        return screenshot.setSelectionMaskColor(value);
    case SettingsColorBinding::CursorGuideLineColor:
        return screenshot.setCursorGuideLineColor(value);
    case SettingsColorBinding::MonitorCenterGuideLineColor:
        return screenshot.setMonitorCenterGuideLineColor(value);
    case SettingsColorBinding::ColorPickerCenterGuideLineColor:
        return screenshot.setColorPickerCenterGuideLineColor(value);
    case SettingsColorBinding::PinBorderColor:
        return storage::PinToScreenSettings().setBorderColor(value);
    }
    return false;
}

QVariant BuiltInSettingsRuntimeBindings::radioValue(SettingsRadioBinding binding) const {
    switch (binding) {
    case SettingsRadioBinding::TrayIcon:
        return storage::TraySettings().icon();
    }
    return {};
}

bool BuiltInSettingsRuntimeBindings::applyRadioValue(SettingsRadioBinding binding,
                                                      const QVariant& value) {
    switch (binding) {
    case SettingsRadioBinding::TrayIcon:
        return storage::TraySettings().setIcon(value.toString());
    }
    return false;
}

QString BuiltInSettingsRuntimeBindings::filePathValue(SettingsFilePathBinding binding) const {
    switch (binding) {
    case SettingsFilePathBinding::TrayCustomIcon:
        return storage::TraySettings().customIcon();
    }
    return {};
}

bool BuiltInSettingsRuntimeBindings::applyFilePathValue(SettingsFilePathBinding binding,
                                                         const QString& value) {
    switch (binding) {
    case SettingsFilePathBinding::TrayCustomIcon:
        return storage::TraySettings().setCustomIcon(value);
    }
    return false;
}

storage::ScreenshotToolbarLayout BuiltInSettingsRuntimeBindings::toolbarLayout() const {
    return storage::ScreenshotToolbarSettings().layout();
}

bool BuiltInSettingsRuntimeBindings::applyToolbarLayout(
    const storage::ScreenshotToolbarLayout& layout) {
    return storage::ScreenshotToolbarSettings().setLayout(layout);
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

QStringList BuiltInSettingsRuntimeBindings::localShortcuts(const QString& toolId) const {
    return storage::DrawingShortcutSettings().shortcuts(toolId);
}

GlobalShortcutValidationResult BuiltInSettingsRuntimeBindings::validateLocalShortcut(
    const QString& toolId, const QString& shortcut) const {
    const QString key = QStringLiteral("drawing_shortcuts/") + toolId;
    const storage::ConfigurationNormalization normalized = storage::ConfigurationSchema::normalize(
        key, QJsonArray{shortcut});
    if (!normalized.valid || normalized.value.toArray().isEmpty()) {
        return {shortcut, false, GlobalShortcutFailureReason::InvalidShortcut};
    }
    const QString canonical = normalized.value.toArray().first().toString();
    if (storage::DrawingShortcutSettings::isReservedShortcut(canonical)) {
        return {canonical, false, GlobalShortcutFailureReason::InvalidShortcut};
    }
    const auto all = storage::DrawingShortcutSettings().allShortcuts();
    for (auto it = all.cbegin(); it != all.cend(); ++it) {
        if (it.key() == toolId) {
            continue;
        }
        for (const QString& existing : it.value()) {
            if (existing.compare(canonical, Qt::CaseInsensitive) == 0) {
                return {canonical, false, GlobalShortcutFailureReason::AlreadyInUse};
            }
        }
    }
    return {canonical, true, GlobalShortcutFailureReason::None};
}

bool BuiltInSettingsRuntimeBindings::applyLocalShortcuts(
    const QString& toolId, const QStringList& shortcuts) {
    for (const QString& shortcut : shortcuts) {
        const GlobalShortcutValidationResult validation =
            validateLocalShortcut(toolId, shortcut);
        if (!validation.supported) {
            return false;
        }
    }
    return storage::DrawingShortcutSettings().setShortcuts(toolId, shortcuts);
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
    case SettingsSectionReset::OtherShortcuts: {
        bool accepted = true;
        const auto resetShortcut = [this, &accepted](GlobalShortcutAction action,
                                                      const QString& key) {
            accepted = applyShortcuts(action, stringListDefault(key)) && accepted;
        };
        resetShortcut(GlobalShortcutAction::ShowOrHideMainWindow,
                      QStringLiteral("global_shortcuts/show_or_hide_main_window"));
        resetShortcut(GlobalShortcutAction::OpenCaptureHistory,
                      QStringLiteral("global_shortcuts/open_capture_history"));
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
                       .toBool()) &&
               storage::ApplicationStorage::instance().configuration().setValues({
                   {QStringLiteral("screenshot/auto_execute_after_text_recognition"),
                    storage::ConfigurationSchema::defaultValue(
                        QStringLiteral("screenshot/auto_execute_after_text_recognition"))},
                   {QStringLiteral("screenshot/double_click_action"),
                    storage::ConfigurationSchema::defaultValue(
                        QStringLiteral("screenshot/double_click_action"))},
                   {QStringLiteral("screenshot/middle_mouse_button_action"),
                    storage::ConfigurationSchema::defaultValue(
                        QStringLiteral("screenshot/middle_mouse_button_action"))},
                   {QStringLiteral("screenshot/auto_save_after_copy"),
                    storage::ConfigurationSchema::defaultValue(
                        QStringLiteral("screenshot/auto_save_after_copy"))},
                   {QStringLiteral("screenshot/copy_image_file_to_clipboard"),
                    storage::ConfigurationSchema::defaultValue(
                        QStringLiteral("screenshot/copy_image_file_to_clipboard"))},
               });
    case SettingsSectionReset::ScreenshotInterfaceSettings:
        return storage::ApplicationStorage::instance().configuration().setValues({
            {QStringLiteral("screenshot_ui/toolbar_size"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/toolbar_size"))},
            {QStringLiteral("screenshot_ui/selection_transition_animation"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/selection_transition_animation"))},
            {QStringLiteral("screenshot_ui/color_picker_display_mode"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/color_picker_display_mode"))},
            {QStringLiteral("screenshot_ui/selection_mask_color"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/selection_mask_color"))},
            {QStringLiteral("screenshot_ui/shortcut_hint_opacity"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/shortcut_hint_opacity"))},
            {QStringLiteral("screenshot_ui/cursor_guide_line_color"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/cursor_guide_line_color"))},
            {QStringLiteral("screenshot_ui/monitor_center_guide_line_color"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/monitor_center_guide_line_color"))},
            {QStringLiteral("screenshot_ui/color_picker_center_guide_line_color"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_ui/color_picker_center_guide_line_color"))},
        });
    case SettingsSectionReset::DrawingToolbar:
        return storage::ApplicationStorage::instance().configuration().setValues({
            {QStringLiteral("screenshot_toolbar/layout"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("screenshot_toolbar/layout"))},
            {QStringLiteral("drawing/quick_selection_disabled_tools"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("drawing/quick_selection_disabled_tools"))},
        });
    case SettingsSectionReset::DrawingShortcuts: {
        QMap<QString, QStringList> defaults;
        for (const QString& toolId : {QStringLiteral("shape"), QStringLiteral("arrow"),
                                     QStringLiteral("brush"), QStringLiteral("highlight"),
                                     QStringLiteral("text"), QStringLiteral("serial_number"),
                                     QStringLiteral("filter"), QStringLiteral("eraser"),
                                     QStringLiteral("watermark")}) {
            defaults.insert(toolId,
                            stringListDefault(QStringLiteral("drawing_shortcuts/") + toolId));
        }
        return storage::DrawingShortcutSettings().setAllShortcutsAtomic(defaults);
    }
    case SettingsSectionReset::PinToScreen:
        return storage::ApplicationStorage::instance().configuration().setValues({
            {QStringLiteral("pin_to_screen/border_color"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("pin_to_screen/border_color"))},
            {QStringLiteral("pin_to_screen/mouse_wheel_zoom_mode"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("pin_to_screen/mouse_wheel_zoom_mode"))},
            {QStringLiteral("pin_to_screen/automatic_text_recognition"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("pin_to_screen/automatic_text_recognition"))},
            {QStringLiteral("pin_to_screen/auto_resize_window"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("pin_to_screen/auto_resize_window"))},
        });
    case SettingsSectionReset::Tray:
        return storage::ApplicationStorage::instance().configuration().setValues({
            {QStringLiteral("tray/enabled"),
             storage::ConfigurationSchema::defaultValue(QStringLiteral("tray/enabled"))},
            {QStringLiteral("tray/icon"),
             storage::ConfigurationSchema::defaultValue(QStringLiteral("tray/icon"))},
            {QStringLiteral("tray/custom_icon"),
             storage::ConfigurationSchema::defaultValue(QStringLiteral("tray/custom_icon"))},
            {QStringLiteral("tray/left_click_action"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("tray/left_click_action"))},
        });
    case SettingsSectionReset::VideoRecording:
        return storage::ApplicationStorage::instance().configuration().setValues({
            {QStringLiteral("video_recording/video_clarity"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/video_clarity"))},
            {QStringLiteral("video_recording/frame_rate"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/frame_rate"))},
            {QStringLiteral("video_recording/animated_image_clarity"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/animated_image_clarity"))},
            {QStringLiteral("video_recording/animated_image_frame_rate"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/animated_image_frame_rate"))},
            {QStringLiteral("video_recording/animated_image_format"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/animated_image_format"))},
            {QStringLiteral("video_recording/encoder"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/encoder"))},
            {QStringLiteral("video_recording/encoding_preset"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/encoding_preset"))},
            {QStringLiteral("video_recording/hide_toolbar_in_recording"),
             storage::ConfigurationSchema::defaultValue(
                 QStringLiteral("video_recording/hide_toolbar_in_recording"))},
        });
    case SettingsSectionReset::GlobalHotkeys:
        return storage::GlobalShortcutSettings().setDisableOnFocusedFullscreenWindow(
            storage::ConfigurationSchema::defaultValue(
                QStringLiteral("global_shortcuts/disable_on_focused_fullscreen_window"))
                .toBool());
    case SettingsSectionReset::SystemGeneral:
        return applyAutoStartAtBoot(
            storage::ConfigurationSchema::defaultValue(
                QStringLiteral("system/auto_start_at_boot"))
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
