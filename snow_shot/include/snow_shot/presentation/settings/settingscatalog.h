#ifndef SNOW_SHOT_PRESENTATION_SETTINGS_SETTINGSCATALOG_H
#define SNOW_SHOT_PRESENTATION_SETTINGS_SETTINGSCATALOG_H

#include "icon_core.h"
#include "snow_shot/presentation/globalshortcutmanager.h"

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariant>

#include <functional>
#include <optional>
#include <variant>

namespace snow_shot::presentation::settings {

struct TranslatableText {
    const char* context = nullptr;
    const char* source = nullptr;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString translated() const;
};

struct SettingsLocation {
    QString pageId;
    QString sectionId;
    QString itemId;

    [[nodiscard]] bool isEmpty() const;
    friend bool operator==(const SettingsLocation& first, const SettingsLocation& second) {
        return first.pageId == second.pageId && first.sectionId == second.sectionId &&
               first.itemId == second.itemId;
    }
    friend bool operator!=(const SettingsLocation& first, const SettingsLocation& second) {
        return !(first == second);
    }
};

enum class SettingsCommandKind {
    CaptureScreenshot,
    Navigate,
};

struct SettingsCommand {
    SettingsCommandKind kind = SettingsCommandKind::Navigate;
    SettingsLocation location;
};

struct SettingsOptionDefinition {
    QVariant value;
    TranslatableText label;
};

enum class SettingsSelectSource {
    Fixed,
    LanguageCatalog,
};

enum class SettingsSelectBinding {
    Theme,
    Language,
    ApplicationPriority,
};

struct SettingsSelectDefinition {
    SettingsSelectBinding binding = SettingsSelectBinding::Theme;
    SettingsSelectSource source = SettingsSelectSource::Fixed;
    QVector<SettingsOptionDefinition> options;
};

enum class SettingsSwitchBinding {
    HistoryEnabled,
    SmartSelection,
};

struct SettingsSwitchDefinition {
    SettingsSwitchBinding binding = SettingsSwitchBinding::HistoryEnabled;
};

enum class SettingsIntegerBinding {
    HistoryRetentionDays,
    HistoryMaxEntries,
    HistoryMaxDiskMiB,
};

struct SettingsIntegerDefinition {
    SettingsIntegerBinding binding = SettingsIntegerBinding::HistoryRetentionDays;
    TranslatableText suffix;
};

struct SettingsShortcutActionDefinition {
    GlobalShortcutAction shortcutAction = GlobalShortcutAction::Screenshot;
    SettingsCommand command;
    std::function<adqt::icons::IconRef()> iconFactory;
};

enum class SettingsActionBinding {
    ClearCaptureHistory,
};

enum class SettingsActionAccent {
    Neutral,
    Danger,
};

struct SettingsConfirmationDefinition {
    TranslatableText title;
    TranslatableText message;
    TranslatableText acceptText;
    TranslatableText rejectText;
};

struct SettingsActionDefinition {
    SettingsActionBinding binding = SettingsActionBinding::ClearCaptureHistory;
    TranslatableText buttonText;
    SettingsActionAccent accent = SettingsActionAccent::Neutral;
    std::function<adqt::icons::IconRef()> iconFactory;
    std::optional<SettingsConfirmationDefinition> confirmation;
};

enum class SettingsCustomRenderer {
    StorageStatus,
};

struct SettingsCustomDefinition {
    SettingsCustomRenderer renderer = SettingsCustomRenderer::StorageStatus;
};

using SettingsItemPayload =
    std::variant<SettingsSelectDefinition, SettingsSwitchDefinition, SettingsIntegerDefinition,
                 SettingsShortcutActionDefinition, SettingsActionDefinition,
                 SettingsCustomDefinition>;

struct SettingsItemDefinition {
    QString id;
    TranslatableText title;
    TranslatableText description;
    QVector<TranslatableText> aliases;
    QString configurationKey;
    SettingsItemPayload payload;
};

enum class SettingsSectionReset {
    None,
    ScreenshotShortcuts,
    OpenSettingsShortcuts,
    GeneralSettings,
    HistoryPolicy,
    ScreenshotSettings,
    SystemSettings,
};

struct SettingsSectionDefinition {
    QString id;
    TranslatableText title;
    TranslatableText searchDescription;
    SettingsSectionReset reset = SettingsSectionReset::None;
    QVector<SettingsItemDefinition> items;
};

enum class SettingsPageKind {
    GeneratedSettings,
    ScreenshotHistory,
};

struct SettingsPageDefinition {
    QString id;
    QString route;
    TranslatableText title;
    TranslatableText description;
    QVector<SettingsSectionDefinition> sections;
    SettingsPageKind kind = SettingsPageKind::GeneratedSettings;
};

struct SettingsNavigationPageDefinition {
    QString id;
    QString pageId;
    std::function<adqt::icons::IconRef()> iconFactory;
};

struct SettingsNavigationGroupDefinition {
    QString id;
    TranslatableText title;
    std::function<adqt::icons::IconRef()> iconFactory;
    QVector<SettingsNavigationPageDefinition> pages;
};

using SettingsNavigationNode =
    std::variant<SettingsNavigationPageDefinition, SettingsNavigationGroupDefinition>;

struct SettingsSectionSummary {
    QString id;
    QString label;
};

class SettingsCatalog final {
  public:
    SettingsCatalog(QVector<SettingsPageDefinition> pages,
                    QVector<SettingsNavigationNode> navigation,
                    SettingsLocation defaultLocation);

    [[nodiscard]] const QVector<SettingsPageDefinition>& pages() const;
    [[nodiscard]] const QVector<SettingsNavigationNode>& navigation() const;
    [[nodiscard]] const SettingsLocation& defaultLocation() const;
    [[nodiscard]] const SettingsPageDefinition* page(const QString& pageId) const;
    [[nodiscard]] const SettingsPageDefinition* pageForRoute(const QString& route) const;
    [[nodiscard]] const SettingsSectionDefinition* section(const QString& pageId,
                                                           const QString& sectionId) const;
    [[nodiscard]] const SettingsItemDefinition* item(const SettingsLocation& location) const;
    [[nodiscard]] std::optional<SettingsCommand>
    commandForShortcut(GlobalShortcutAction action) const;
    [[nodiscard]] SettingsLocation resolveLocation(const SettingsLocation& requested) const;
    [[nodiscard]] QVector<SettingsSectionSummary> sectionSummaries(const QString& pageId) const;
    [[nodiscard]] QStringList validationErrors() const;

  private:
    QVector<SettingsPageDefinition> m_pages;
    QVector<SettingsNavigationNode> m_navigation;
    SettingsLocation m_defaultLocation;
};

[[nodiscard]] SettingsCatalog buildBuiltInSettingsCatalog();
[[nodiscard]] const SettingsCatalog& builtInSettingsCatalog();
[[nodiscard]] QString generatedObjectName(const QString& prefix, const QString& stableId);

} // namespace snow_shot::presentation::settings

Q_DECLARE_METATYPE(snow_shot::presentation::settings::SettingsLocation)
Q_DECLARE_METATYPE(snow_shot::presentation::settings::SettingsCommand)

#endif // SNOW_SHOT_PRESENTATION_SETTINGS_SETTINGSCATALOG_H
