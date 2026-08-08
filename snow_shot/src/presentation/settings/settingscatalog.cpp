#include "snow_shot/presentation/settings/settingscatalog.h"

#include "antd_icons.h"
#include "snow_shot/presentation/components/icons/snowshoticons.h"
#include "snow_shot/storage/configurationschema.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace snow_shot::presentation::settings {
namespace {
namespace outlined_icons = adqt::icons::antd::outlined;
namespace custom_twotone_icons = snow_shot::presentation::icons::custom::twotone;

constexpr TranslatableText settingsText(const char* source) {
    return {"SettingsCatalog", source};
}

constexpr auto QUICK_PAGE_ID = "quick-functions";
constexpr auto HISTORY_PAGE_ID = "screenshot-history";
constexpr auto FUNCTION_PAGE_ID = "function-settings";
constexpr auto INTERFACE_PAGE_ID = "interface-settings";
constexpr auto STORAGE_PAGE_ID = "storage-and-privacy";
constexpr auto SYSTEM_PAGE_ID = "system-settings";

SettingsItemDefinition screenshotItem() {
    SettingsShortcutActionDefinition payload;
    payload.shortcutAction = GlobalShortcutAction::Screenshot;
    payload.command = {SettingsCommandKind::CaptureScreenshot, {}};
    payload.iconFactory = []() { return custom_twotone_icons::ScreenshotFeature(); };
    return {
        QStringLiteral("quick.screenshot"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Screenshot")),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Take a screenshot")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Capture")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Screen capture"))},
        QStringLiteral("global_shortcuts/screenshot"),
        payload,
    };
}

SettingsItemDefinition openSettingsItem() {
    SettingsShortcutActionDefinition payload;
    payload.shortcutAction = GlobalShortcutAction::OpenSettings;
    payload.command = {
        SettingsCommandKind::Navigate,
        {QString::fromLatin1(INTERFACE_PAGE_ID), QStringLiteral("general"), {}},
    };
    payload.iconFactory = []() { return outlined_icons::Control(); };
    return {
        QStringLiteral("quick.open-interface-settings"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Open Interface Settings")),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog",
                                       "Open the application interface settings panel")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Preferences"))},
        QStringLiteral("global_shortcuts/open_settings"),
        payload,
    };
}

SettingsItemDefinition themeItem() {
    SettingsSelectDefinition payload;
    payload.options = {
        {QStringLiteral("system"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Follow System"))},
        {QStringLiteral("light"), settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Light"))},
        {QStringLiteral("dark"), settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Dark"))},
    };
    return {
        QStringLiteral("interface.theme"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Theme")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Match your system appearance or choose a light or dark theme")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Appearance")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Color mode"))},
        QStringLiteral("interface/theme_mode"),
        payload,
    };
}

SettingsItemDefinition languageItem() {
    SettingsSelectDefinition payload;
    payload.binding = SettingsSelectBinding::Language;
    payload.source = SettingsSelectSource::LanguageCatalog;
    payload.options = {
        {QStringLiteral("system"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Follow System"))},
    };
    return {
        QStringLiteral("interface.language"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Language")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Select the language used throughout the application")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Locale")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Translation"))},
        QStringLiteral("interface/language"),
        payload,
    };
}

SettingsItemDefinition applicationPriorityItem() {
    SettingsSelectDefinition payload;
    payload.options = {
        {QStringLiteral("normal"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Normal"))},
        {QStringLiteral("above_normal"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Above normal"))},
        {QStringLiteral("high"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "High"))},
        {QStringLiteral("real_time"),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Real-time"))},
    };
    payload.binding = SettingsSelectBinding::ApplicationPriority;
    return {
        QStringLiteral("system.application-priority"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Application priority")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Choose how much execution time the application receives")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Process priority")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Execution order"))},
        QStringLiteral("system/application_priority"),
        payload,
    };
}

SettingsItemDefinition historyEnabledItem() {
    return {
        QStringLiteral("history.enabled"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Persistent capture history")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Keep captures available after the application closes")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Save history"))},
        QStringLiteral("capture_history/enabled"),
        SettingsSwitchDefinition{},
    };
}

SettingsItemDefinition smartSelectionItem() {
    return {
        QStringLiteral("screenshot.smart-selection"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Smart Selection")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog",
            "Select child elements within a window while taking a screenshot")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Child elements")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "MSAA"))},
        QStringLiteral("screenshot_selection/smart_selection"),
        SettingsSwitchDefinition{SettingsSwitchBinding::SmartSelection},
    };
}

SettingsItemDefinition historyIntegerItem(const QString& id, TranslatableText title,
                                           TranslatableText description, const QString& key,
                                           SettingsIntegerBinding binding,
                                           TranslatableText suffix,
                                           QVector<TranslatableText> aliases = {}) {
    return {
        id,
        title,
        description,
        aliases,
        key,
        SettingsIntegerDefinition{binding, suffix},
    };
}

SettingsItemDefinition clearHistoryItem() {
    SettingsActionDefinition payload;
    payload.buttonText = settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Clear history"));
    payload.accent = SettingsActionAccent::Danger;
    payload.iconFactory = []() { return outlined_icons::Rest(); };
    payload.confirmation = {
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Clear capture history?")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "All capture history and quarantined records will be removed")),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Clear history")),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Cancel")),
    };
    return {
        QStringLiteral("history.clear"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Clear capture history")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Permanently remove all saved captures and quarantined records")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Delete history")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Remove captures"))},
        {},
        payload,
    };
}

SettingsItemDefinition storageStatusItem() {
    return {
        QStringLiteral("storage.status"),
        settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Storage status")),
        settingsText(QT_TRANSLATE_NOOP(
            "SettingsCatalog", "Current storage location, mode, usage, and latest errors")),
        {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Disk usage")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Storage location")),
         settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Storage error"))},
        {},
        SettingsCustomDefinition{},
    };
}

QVector<SettingsPageDefinition> builtInPages() {
    return {
        {
            QString::fromLatin1(QUICK_PAGE_ID),
            QStringLiteral("/"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Quick Functions")),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Quick Functions page")),
            {
                {
                    QStringLiteral("screenshot"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Screenshot")),
                    settingsText(QT_TRANSLATE_NOOP(
                        "SettingsCatalog", "Screenshot shortcuts and actions")),
                    SettingsSectionReset::ScreenshotShortcuts,
                    {screenshotItem()},
                },
                {
                    QStringLiteral("other"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Other")),
                    settingsText(QT_TRANSLATE_NOOP(
                        "SettingsCatalog", "Other application shortcuts and actions")),
                    SettingsSectionReset::OpenSettingsShortcuts,
                    {openSettingsItem()},
                },
            },
        },
        {
            QString::fromLatin1(HISTORY_PAGE_ID),
            QStringLiteral("/history"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Screenshot History")),
            settingsText(QT_TRANSLATE_NOOP(
                "SettingsCatalog", "Preview and manage saved screenshot history")),
            {},
            SettingsPageKind::ScreenshotHistory,
        },
        {
            QString::fromLatin1(FUNCTION_PAGE_ID),
            QStringLiteral("/settings/functionSettings"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Function Settings")),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Configure screenshot behavior")),
            {
                {
                    QStringLiteral("screenshot-settings"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Screenshot")),
                    settingsText(QT_TRANSLATE_NOOP(
                        "SettingsCatalog", "Screenshot selection behavior")),
                    SettingsSectionReset::ScreenshotSettings,
                    {smartSelectionItem()},
                },
            },
        },
        {
            QString::fromLatin1(INTERFACE_PAGE_ID),
            QStringLiteral("/settings/generalSettings"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Interface Settings")),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Interface Settings page")),
            {
                {
                    QStringLiteral("general"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "General")),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog",
                                                   "Appearance and language settings")),
                    SettingsSectionReset::GeneralSettings,
                    {themeItem(), languageItem()},
                },
            },
        },
        {
            QString::fromLatin1(STORAGE_PAGE_ID),
            QStringLiteral("/settings/storageAndPrivacy"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Storage and Privacy")),
            settingsText(
                QT_TRANSLATE_NOOP("SettingsCatalog", "Storage and Privacy settings page")),
            {
                {
                    QStringLiteral("history"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "History")),
                    settingsText(QT_TRANSLATE_NOOP(
                        "SettingsCatalog", "Capture history retention and cleanup settings")),
                    SettingsSectionReset::HistoryPolicy,
                    {
                        historyEnabledItem(),
                        historyIntegerItem(
                            QStringLiteral("history.retention-days"),
                            settingsText(
                                QT_TRANSLATE_NOOP("SettingsCatalog", "Retention period")),
                            settingsText(QT_TRANSLATE_NOOP(
                                "SettingsCatalog", "Delete captures after they reach this age")),
                            QStringLiteral("capture_history/retention_days"),
                            SettingsIntegerBinding::HistoryRetentionDays,
                            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", " days")),
                            {settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Age"))}),
                        historyIntegerItem(
                            QStringLiteral("history.max-entries"),
                            settingsText(
                                QT_TRANSLATE_NOOP("SettingsCatalog", "Maximum entries")),
                            settingsText(QT_TRANSLATE_NOOP(
                                "SettingsCatalog",
                                "Remove the oldest captures when this limit is exceeded")),
                            QStringLiteral("capture_history/max_entries"),
                            SettingsIntegerBinding::HistoryMaxEntries, {},
                            {settingsText(
                                QT_TRANSLATE_NOOP("SettingsCatalog", "Capture count"))}),
                        historyIntegerItem(
                            QStringLiteral("history.max-disk-mib"),
                            settingsText(
                                QT_TRANSLATE_NOOP("SettingsCatalog", "Maximum disk usage")),
                            settingsText(QT_TRANSLATE_NOOP(
                                "SettingsCatalog",
                                "Limit how much disk space capture history can use")),
                            QStringLiteral("capture_history/max_disk_mib"),
                            SettingsIntegerBinding::HistoryMaxDiskMiB,
                            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", " MiB")),
                            {settingsText(
                                QT_TRANSLATE_NOOP("SettingsCatalog", "Disk limit"))}),
                        clearHistoryItem(),
                    },
                },
                {
                    QStringLiteral("storage-status"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Storage Status")),
                    settingsText(QT_TRANSLATE_NOOP(
                        "SettingsCatalog", "Current storage location, mode, usage, and errors")),
                    SettingsSectionReset::None,
                    {storageStatusItem()},
                },
            },
        },
        {
            QString::fromLatin1(SYSTEM_PAGE_ID),
            QStringLiteral("/settings/systemSettings"),
            settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "System Settings")),
            settingsText(QT_TRANSLATE_NOOP(
                "SettingsCatalog", "Configure application process behavior")),
            {
                {
                    QStringLiteral("core"),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Core")),
                    settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Core application settings")),
                    SettingsSectionReset::SystemSettings,
                    {applicationPriorityItem()},
                },
            },
        },
    };
}

QVector<SettingsNavigationNode> builtInNavigation() {
    SettingsNavigationPageDefinition quick;
    quick.id = QStringLiteral("nav.quick-functions");
    quick.pageId = QString::fromLatin1(QUICK_PAGE_ID);
    quick.iconFactory = []() { return outlined_icons::Thunderbolt(); };

    SettingsNavigationPageDefinition history;
    history.id = QStringLiteral("nav.screenshot-history");
    history.pageId = QString::fromLatin1(HISTORY_PAGE_ID);
    history.iconFactory = []() { return outlined_icons::History(); };

    SettingsNavigationGroupDefinition settingsGroup;
    settingsGroup.id = QStringLiteral("nav.settings");
    settingsGroup.title = settingsText(QT_TRANSLATE_NOOP("SettingsCatalog", "Settings"));
    settingsGroup.iconFactory = []() { return outlined_icons::Setting(); };
    settingsGroup.pages = {
        {
            QStringLiteral("nav.interface-settings"),
            QString::fromLatin1(INTERFACE_PAGE_ID),
            []() { return outlined_icons::Control(); },
        },
        {
            QStringLiteral("nav.function-settings"),
            QString::fromLatin1(FUNCTION_PAGE_ID),
            []() { return outlined_icons::Function(); },
        },
        {
            QStringLiteral("nav.storage-and-privacy"),
            QString::fromLatin1(STORAGE_PAGE_ID),
            []() { return outlined_icons::Lock(); },
        },
        {
            QStringLiteral("nav.system-settings"),
            QString::fromLatin1(SYSTEM_PAGE_ID),
            []() { return outlined_icons::Control(); },
        },
    };
    return {quick, history, settingsGroup};
}

QString locationText(const SettingsLocation& location) {
    return QStringLiteral("%1/%2/%3").arg(location.pageId, location.sectionId, location.itemId);
}

void addUnique(QStringList* errors, QSet<QString>* values, const QString& value,
               const QString& kind) {
    if (value.trimmed().isEmpty()) {
        errors->push_back(QStringLiteral("%1 must not be empty").arg(kind));
    } else if (values->contains(value)) {
        errors->push_back(QStringLiteral("duplicate %1: %2").arg(kind, value));
    } else {
        values->insert(value);
    }
}

} // namespace

bool TranslatableText::isValid() const {
    return context != nullptr && source != nullptr && *context != '\0' && *source != '\0';
}

QString TranslatableText::translated() const {
    return isValid() ? QCoreApplication::translate(context, source) : QString();
}

bool SettingsLocation::isEmpty() const {
    return pageId.trimmed().isEmpty();
}

SettingsCatalog::SettingsCatalog(QVector<SettingsPageDefinition> pages,
                                 QVector<SettingsNavigationNode> navigation,
                                 SettingsLocation defaultLocation)
    : m_pages(std::move(pages)), m_navigation(std::move(navigation)),
      m_defaultLocation(std::move(defaultLocation)) {}

const QVector<SettingsPageDefinition>& SettingsCatalog::pages() const {
    return m_pages;
}

const QVector<SettingsNavigationNode>& SettingsCatalog::navigation() const {
    return m_navigation;
}

const SettingsLocation& SettingsCatalog::defaultLocation() const {
    return m_defaultLocation;
}

const SettingsPageDefinition* SettingsCatalog::page(const QString& pageId) const {
    const auto found = std::find_if(m_pages.cbegin(), m_pages.cend(), [&pageId](const auto& item) {
        return item.id == pageId;
    });
    return found == m_pages.cend() ? nullptr : &*found;
}

const SettingsPageDefinition* SettingsCatalog::pageForRoute(const QString& route) const {
    const auto found = std::find_if(m_pages.cbegin(), m_pages.cend(), [&route](const auto& item) {
        return item.route == route;
    });
    return found == m_pages.cend() ? nullptr : &*found;
}

const SettingsSectionDefinition* SettingsCatalog::section(const QString& pageId,
                                                          const QString& sectionId) const {
    const SettingsPageDefinition* foundPage = page(pageId);
    if (foundPage == nullptr) {
        return nullptr;
    }
    const auto found =
        std::find_if(foundPage->sections.cbegin(), foundPage->sections.cend(),
                     [&sectionId](const auto& item) { return item.id == sectionId; });
    return found == foundPage->sections.cend() ? nullptr : &*found;
}

const SettingsItemDefinition* SettingsCatalog::item(const SettingsLocation& location) const {
    const SettingsSectionDefinition* foundSection = section(location.pageId, location.sectionId);
    if (foundSection == nullptr || location.itemId.isEmpty()) {
        return nullptr;
    }
    const auto found = std::find_if(foundSection->items.cbegin(), foundSection->items.cend(),
                                    [&location](const auto& item) {
                                        return item.id == location.itemId;
                                    });
    return found == foundSection->items.cend() ? nullptr : &*found;
}

std::optional<SettingsCommand>
SettingsCatalog::commandForShortcut(GlobalShortcutAction action) const {
    for (const SettingsPageDefinition& pageDefinition : m_pages) {
        for (const SettingsSectionDefinition& sectionDefinition : pageDefinition.sections) {
            for (const SettingsItemDefinition& itemDefinition : sectionDefinition.items) {
                const auto* shortcut =
                    std::get_if<SettingsShortcutActionDefinition>(&itemDefinition.payload);
                if (shortcut != nullptr && shortcut->shortcutAction == action) {
                    return shortcut->command;
                }
            }
        }
    }
    return std::nullopt;
}

SettingsLocation SettingsCatalog::resolveLocation(const SettingsLocation& requested) const {
    const SettingsPageDefinition* foundPage = page(requested.pageId);
    if (foundPage == nullptr) {
        return m_defaultLocation;
    }
    SettingsLocation resolved{foundPage->id, {}, {}};
    const SettingsSectionDefinition* foundSection = section(foundPage->id, requested.sectionId);
    if (foundSection == nullptr) {
        if (foundPage->sections.isEmpty()) {
            return foundPage->kind == SettingsPageKind::ScreenshotHistory ? resolved
                                                                           : m_defaultLocation;
        }
        foundSection = &foundPage->sections.constFirst();
    }
    resolved.sectionId = foundSection->id;
    if (!requested.itemId.isEmpty()) {
        const SettingsLocation itemLocation{resolved.pageId, resolved.sectionId, requested.itemId};
        if (item(itemLocation) != nullptr) {
            resolved.itemId = requested.itemId;
        }
    }
    return resolved;
}

QVector<SettingsSectionSummary> SettingsCatalog::sectionSummaries(const QString& pageId) const {
    QVector<SettingsSectionSummary> result;
    const SettingsPageDefinition* foundPage = page(pageId);
    if (foundPage == nullptr) {
        return result;
    }
    result.reserve(foundPage->sections.size());
    for (const SettingsSectionDefinition& sectionDefinition : foundPage->sections) {
        result.push_back({sectionDefinition.id, sectionDefinition.title.translated()});
    }
    return result;
}

QStringList SettingsCatalog::validationErrors() const {
    QStringList errors;
    QSet<QString> pageIds;
    QSet<QString> routes;
    QSet<QString> sectionIds;
    QSet<QString> itemIds;
    QSet<QString> navigationIds;
    QSet<QString> searchIds;
    QSet<QString> objectNames;
    QSet<GlobalShortcutAction> shortcutActions;

    for (const SettingsPageDefinition& pageDefinition : m_pages) {
        addUnique(&errors, &pageIds, pageDefinition.id, QStringLiteral("page id"));
        addUnique(&errors, &routes, pageDefinition.route, QStringLiteral("route"));
        if (!pageDefinition.route.startsWith(u'/')) {
            errors.push_back(QStringLiteral("page route must be absolute: %1")
                                 .arg(pageDefinition.route));
        }
        if (pageDefinition.sections.isEmpty() &&
            pageDefinition.kind == SettingsPageKind::GeneratedSettings) {
            errors.push_back(QStringLiteral("page has no sections: %1").arg(pageDefinition.id));
        }
        addUnique(&errors, &searchIds, QStringLiteral("page:%1").arg(pageDefinition.id),
                  QStringLiteral("generated search id"));
        addUnique(&errors, &objectNames,
                  generatedObjectName(QStringLiteral("settings-page"), pageDefinition.id),
                  QStringLiteral("generated object name"));
        if (!pageDefinition.title.isValid() || !pageDefinition.description.isValid()) {
            errors.push_back(QStringLiteral("page text is incomplete: %1").arg(pageDefinition.id));
        }
        for (const SettingsSectionDefinition& sectionDefinition : pageDefinition.sections) {
            addUnique(&errors, &sectionIds, sectionDefinition.id,
                      QStringLiteral("section id"));
            addUnique(&errors, &searchIds,
                      QStringLiteral("section:%1/%2")
                          .arg(pageDefinition.id, sectionDefinition.id),
                      QStringLiteral("generated search id"));
            addUnique(&errors, &objectNames,
                      generatedObjectName(
                          QStringLiteral("settings-section"),
                          QStringLiteral("%1-%2").arg(pageDefinition.id, sectionDefinition.id)),
                      QStringLiteral("generated object name"));
            if (!sectionDefinition.title.isValid() ||
                !sectionDefinition.searchDescription.isValid()) {
                errors.push_back(QStringLiteral("section text is incomplete: %1/%2")
                                     .arg(pageDefinition.id, sectionDefinition.id));
            }
            if (sectionDefinition.items.isEmpty()) {
                errors.push_back(QStringLiteral("section has no items: %1/%2")
                                     .arg(pageDefinition.id, sectionDefinition.id));
            }
            for (const SettingsItemDefinition& itemDefinition : sectionDefinition.items) {
                addUnique(&errors, &itemIds, itemDefinition.id, QStringLiteral("item id"));
                addUnique(&errors, &searchIds,
                          QStringLiteral("item:%1").arg(itemDefinition.id),
                          QStringLiteral("generated search id"));
                addUnique(&errors, &objectNames,
                          generatedObjectName(QStringLiteral("settings-item"),
                                              itemDefinition.id),
                          QStringLiteral("generated object name"));
                if (!itemDefinition.title.isValid() || !itemDefinition.description.isValid()) {
                    errors.push_back(QStringLiteral("item text is incomplete: %1")
                                         .arg(itemDefinition.id));
                }
                for (const TranslatableText& alias : itemDefinition.aliases) {
                    if (!alias.isValid()) {
                        errors.push_back(QStringLiteral("item alias text is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                const auto* schemaEntry = itemDefinition.configurationKey.isEmpty()
                                              ? nullptr
                                              : storage::ConfigurationSchema::entry(
                                                    itemDefinition.configurationKey);
                if (!itemDefinition.configurationKey.isEmpty() && schemaEntry == nullptr) {
                    errors.push_back(QStringLiteral("unknown configuration key for %1: %2")
                                         .arg(itemDefinition.id,
                                              itemDefinition.configurationKey));
                }
                if (const auto* select = std::get_if<SettingsSelectDefinition>(
                        &itemDefinition.payload);
                    select != nullptr) {
                    QString expectedKey;
                    SettingsSelectSource expectedSource = SettingsSelectSource::Fixed;
                    switch (select->binding) {
                    case SettingsSelectBinding::Theme:
                        expectedKey = QStringLiteral("interface/theme_mode");
                        break;
                    case SettingsSelectBinding::Language:
                        expectedKey = QStringLiteral("interface/language");
                        expectedSource = SettingsSelectSource::LanguageCatalog;
                        break;
                    case SettingsSelectBinding::ApplicationPriority:
                        expectedKey = QStringLiteral("system/application_priority");
                        break;
                    }
                    if (schemaEntry == nullptr ||
                        schemaEntry->valueKind != storage::ConfigurationValueKind::String ||
                        select->options.isEmpty()) {
                        errors.push_back(QStringLiteral("select item is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                    if (itemDefinition.configurationKey != expectedKey ||
                        select->source != expectedSource) {
                        errors.push_back(QStringLiteral("select binding is incompatible: %1")
                                             .arg(itemDefinition.id));
                    }
                    QSet<QString> configuredValues;
                    for (const SettingsOptionDefinition& option : select->options) {
                        configuredValues.insert(option.value.toString());
                        if (!option.label.isValid()) {
                            errors.push_back(QStringLiteral("select option text is incomplete: %1")
                                                 .arg(itemDefinition.id));
                        }
                    }
                    QSet<QString> allowed;
                    if (schemaEntry != nullptr) {
                        for (const QString& allowedValue : schemaEntry->allowedStringValues) {
                            allowed.insert(allowedValue);
                        }
                    }
                    if (!allowed.isEmpty() && configuredValues != allowed) {
                        errors.push_back(QStringLiteral("select options do not match schema: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                if (const auto* switchDefinition =
                        std::get_if<SettingsSwitchDefinition>(&itemDefinition.payload)) {
                    QString expectedKey;
                    switch (switchDefinition->binding) {
                    case SettingsSwitchBinding::HistoryEnabled:
                        expectedKey = QStringLiteral("capture_history/enabled");
                        break;
                    case SettingsSwitchBinding::SmartSelection:
                        expectedKey = QStringLiteral("screenshot_selection/smart_selection");
                        break;
                    }
                    if (itemDefinition.configurationKey != expectedKey || schemaEntry == nullptr ||
                        schemaEntry->valueKind != storage::ConfigurationValueKind::Boolean) {
                        errors.push_back(QStringLiteral("switch binding is incompatible: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                if (const auto* integer =
                        std::get_if<SettingsIntegerDefinition>(&itemDefinition.payload)) {
                    QString expectedKey;
                    switch (integer->binding) {
                    case SettingsIntegerBinding::HistoryRetentionDays:
                        expectedKey = QStringLiteral("capture_history/retention_days");
                        break;
                    case SettingsIntegerBinding::HistoryMaxEntries:
                        expectedKey = QStringLiteral("capture_history/max_entries");
                        break;
                    case SettingsIntegerBinding::HistoryMaxDiskMiB:
                        expectedKey = QStringLiteral("capture_history/max_disk_mib");
                        break;
                    }
                    if (itemDefinition.configurationKey != expectedKey || schemaEntry == nullptr ||
                        schemaEntry->valueKind != storage::ConfigurationValueKind::Integer ||
                        !schemaEntry->integerRange.has_value()) {
                        errors.push_back(QStringLiteral("integer binding is incompatible: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                if (const auto* shortcut =
                        std::get_if<SettingsShortcutActionDefinition>(&itemDefinition.payload)) {
                    const QString expectedKey =
                        shortcut->shortcutAction == GlobalShortcutAction::Screenshot
                            ? QStringLiteral("global_shortcuts/screenshot")
                            : QStringLiteral("global_shortcuts/open_settings");
                    if (schemaEntry == nullptr ||
                        schemaEntry->valueKind != storage::ConfigurationValueKind::StringList ||
                        schemaEntry->maximumListItems != 2 || !shortcut->iconFactory ||
                        itemDefinition.configurationKey != expectedKey) {
                        errors.push_back(QStringLiteral("shortcut item is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                    if (shortcutActions.contains(shortcut->shortcutAction)) {
                        errors.push_back(QStringLiteral("duplicate shortcut action: %1")
                                             .arg(itemDefinition.id));
                    } else {
                        shortcutActions.insert(shortcut->shortcutAction);
                    }
                    if (shortcut->command.kind == SettingsCommandKind::Navigate) {
                        if (shortcut->command.location.isEmpty() ||
                            resolveLocation(shortcut->command.location) !=
                                shortcut->command.location) {
                            errors.push_back(QStringLiteral("shortcut navigation is invalid: %1")
                                                 .arg(itemDefinition.id));
                        }
                    } else if (shortcut->command.location != SettingsLocation{}) {
                        errors.push_back(QStringLiteral("shortcut command location is unexpected: %1")
                                             .arg(itemDefinition.id));
                    }
                    const SettingsCommandKind expectedCommand =
                        shortcut->shortcutAction == GlobalShortcutAction::Screenshot
                            ? SettingsCommandKind::CaptureScreenshot
                            : SettingsCommandKind::Navigate;
                    if (shortcut->command.kind != expectedCommand) {
                        errors.push_back(QStringLiteral("shortcut command is incompatible: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                if (const auto* action =
                        std::get_if<SettingsActionDefinition>(&itemDefinition.payload)) {
                    if (!itemDefinition.configurationKey.isEmpty() ||
                        !action->buttonText.isValid() || !action->iconFactory) {
                        errors.push_back(QStringLiteral("action item is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                    if (action->confirmation.has_value() &&
                        (!action->confirmation->title.isValid() ||
                         !action->confirmation->message.isValid() ||
                         !action->confirmation->acceptText.isValid() ||
                         !action->confirmation->rejectText.isValid())) {
                        errors.push_back(QStringLiteral("action confirmation is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                }
                if (const auto* custom =
                        std::get_if<SettingsCustomDefinition>(&itemDefinition.payload)) {
                    const bool rendererSupported =
                        custom->renderer == SettingsCustomRenderer::StorageStatus;
                    if (!itemDefinition.configurationKey.isEmpty() || !rendererSupported) {
                        errors.push_back(QStringLiteral("custom item is incomplete: %1")
                                             .arg(itemDefinition.id));
                    }
                }
            }
        }
    }

    QSet<QString> navigatedPages;
    const auto validateNavigationPage = [&](const SettingsNavigationPageDefinition& navPage) {
        addUnique(&errors, &navigationIds, navPage.id, QStringLiteral("navigation id"));
        if (page(navPage.pageId) == nullptr) {
            errors.push_back(QStringLiteral("navigation references unknown page: %1")
                                 .arg(navPage.pageId));
        } else if (navigatedPages.contains(navPage.pageId)) {
            errors.push_back(QStringLiteral("page appears more than once in navigation: %1")
                                 .arg(navPage.pageId));
        } else {
            navigatedPages.insert(navPage.pageId);
        }
        if (!navPage.iconFactory) {
            errors.push_back(QStringLiteral("navigation icon factory is missing: %1")
                                 .arg(navPage.id));
        }
    };
    for (const SettingsNavigationNode& node : m_navigation) {
        if (const auto* navPage = std::get_if<SettingsNavigationPageDefinition>(&node)) {
            validateNavigationPage(*navPage);
        } else if (const auto* group = std::get_if<SettingsNavigationGroupDefinition>(&node)) {
            addUnique(&errors, &navigationIds, group->id, QStringLiteral("navigation id"));
            if (!group->title.isValid() || !group->iconFactory || group->pages.isEmpty()) {
                errors.push_back(QStringLiteral("navigation group is incomplete: %1")
                                     .arg(group->id));
            }
            for (const SettingsNavigationPageDefinition& groupedPage : group->pages) {
                validateNavigationPage(groupedPage);
            }
        }
    }
    for (const SettingsPageDefinition& pageDefinition : m_pages) {
        if (!navigatedPages.contains(pageDefinition.id)) {
            errors.push_back(QStringLiteral("page is absent from navigation: %1")
                                 .arg(pageDefinition.id));
        }
    }

    const SettingsPageDefinition* defaultPage = page(m_defaultLocation.pageId);
    const SettingsSectionDefinition* defaultSection =
        section(m_defaultLocation.pageId, m_defaultLocation.sectionId);
    const bool defaultItemValid =
        m_defaultLocation.itemId.isEmpty() || item(m_defaultLocation) != nullptr;
    if (m_defaultLocation.isEmpty() || defaultPage == nullptr || defaultSection == nullptr ||
        !defaultItemValid) {
        errors.push_back(QStringLiteral("invalid default location: %1")
                             .arg(locationText(m_defaultLocation)));
    }
    return errors;
}

SettingsCatalog buildBuiltInSettingsCatalog() {
    return {builtInPages(), builtInNavigation(),
            {QString::fromLatin1(QUICK_PAGE_ID), QStringLiteral("screenshot"),
             QStringLiteral("quick.screenshot")}};
}

const SettingsCatalog& builtInSettingsCatalog() {
    static const SettingsCatalog catalog = buildBuiltInSettingsCatalog();
    static const bool validated = []() {
        const QStringList errors = catalog.validationErrors();
        if (!errors.isEmpty()) {
            qFatal("Invalid built-in settings catalog:\n%s", qPrintable(errors.join(u'\n')));
        }
        return true;
    }();
    Q_UNUSED(validated)
    return catalog;
}

QString generatedObjectName(const QString& prefix, const QString& stableId) {
    QString result = stableId.toLower();
    result.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    result.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return QStringLiteral("%1-%2").arg(prefix, result);
}

} // namespace snow_shot::presentation::settings
