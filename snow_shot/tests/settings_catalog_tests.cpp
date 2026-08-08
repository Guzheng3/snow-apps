#include "snow_shot/presentation/settings/settingscatalog.h"
#include "snow_shot/presentation/settings/settingssearchindex.h"
#include "snow_shot/storage/configurationschema.h"

#include "antd_icons.h"

#include <QCoreApplication>
#include <QSet>
#include <QTranslator>

#include <cstdlib>
#include <iostream>

namespace settings = snow_shot::presentation::settings;
namespace storage = snow_shot::storage;

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

settings::TranslatableText text(const char* source) {
    return {"SettingsCatalogTests", source};
}

bool insertUnique(QSet<QString>* values, const QString& value) {
    if (values->contains(value)) {
        return false;
    }
    values->insert(value);
    return true;
}

class CatalogTranslator final : public QTranslator {
  public:
    QString translate(const char* context, const char* sourceText, const char*, int) const override {
        if (QString::fromLatin1(context) != QStringLiteral("SettingsCatalog")) {
            return {};
        }
        const QString source = QString::fromUtf8(sourceText);
        if (source == QStringLiteral("Theme")) {
            return QStringLiteral("Localized Theme");
        }
        if (source == QStringLiteral("Dark")) {
            return QStringLiteral("Night Mode");
        }
        if (source == QStringLiteral("Appearance")) {
            return QStringLiteral("Visual Style");
        }
        return {};
    }
};

void builtInCatalogIsCompleteAndValid() {
    const settings::SettingsCatalog& catalog = settings::builtInSettingsCatalog();
    require(catalog.validationErrors().isEmpty(), "built-in settings catalog must validate");
    require(catalog.pages().size() == 5, "catalog must contain five pages");

    qsizetype sectionCount = 0;
    qsizetype itemCount = 0;
    QSet<QString> objectNames;
    for (const auto& page : catalog.pages()) {
        sectionCount += page.sections.size();
        require(!page.id.isEmpty() && !page.route.isEmpty() && page.title.isValid() &&
                    page.description.isValid(),
                "page metadata must be complete");
        require(insertUnique(
                    &objectNames,
                    settings::generatedObjectName(QStringLiteral("settings-page"), page.id)),
                "generated page object names must be unique");
        for (const auto& section : page.sections) {
            itemCount += section.items.size();
            require(insertUnique(&objectNames,
                                 settings::generatedObjectName(
                                     QStringLiteral("settings-section"),
                                     QStringLiteral("%1-%2").arg(page.id, section.id))),
                    "generated section object names must be unique");
            for (const auto& item : section.items) {
                require(insertUnique(
                            &objectNames,
                            settings::generatedObjectName(QStringLiteral("settings-item"),
                                                          item.id)),
                        "generated item object names must be unique");
                if (!item.configurationKey.isEmpty()) {
                    require(storage::ConfigurationSchema::entry(item.configurationKey) != nullptr,
                            "catalog persistence keys must resolve through ConfigurationSchema");
                }
            }
        }
    }
    require(sectionCount == 6 && itemCount == 11,
            "catalog must contain the expected six sections and eleven items");
    const auto* functionPage = catalog.page(QStringLiteral("function-settings"));
    const auto* smartSelection = catalog.item(
        {QStringLiteral("function-settings"), QStringLiteral("screenshot-settings"),
         QStringLiteral("screenshot.smart-selection")});
    require(functionPage != nullptr && functionPage->route ==
                QStringLiteral("/settings/functionSettings") && smartSelection != nullptr &&
                smartSelection->configurationKey ==
                    QStringLiteral("screenshot_selection/smart_selection") &&
                std::get<settings::SettingsSwitchDefinition>(smartSelection->payload).binding ==
                    settings::SettingsSwitchBinding::SmartSelection,
            "Function Settings must expose the persisted Smart Selection switch");

    const auto* settingsGroup =
        std::get_if<settings::SettingsNavigationGroupDefinition>(&catalog.navigation().at(2));
    require(settingsGroup != nullptr && settingsGroup->pages.size() >= 2 &&
                settingsGroup->pages.at(0).pageId == QStringLiteral("interface-settings") &&
                settingsGroup->pages.at(1).pageId == QStringLiteral("function-settings"),
            "Function Settings must appear below Interface Settings in the Settings navigation");

    const auto* retention =
        storage::ConfigurationSchema::entry(QStringLiteral("capture_history/retention_days"));
    const auto* shortcuts =
        storage::ConfigurationSchema::entry(QStringLiteral("global_shortcuts/screenshot"));
    require(retention != nullptr && retention->valueKind == storage::ConfigurationValueKind::Integer &&
                retention->integerRange.has_value() && retention->integerRange->minimum == 1 &&
                retention->integerRange->maximum == 365,
            "integer schema metadata must expose renderer constraints");
    require(shortcuts != nullptr &&
                shortcuts->valueKind == storage::ConfigurationValueKind::StringList &&
                shortcuts->maximumListItems == 2,
            "shortcut schema metadata must expose list limits");
}

void structuredFallbackIsDeterministic() {
    const settings::SettingsCatalog& catalog = settings::builtInSettingsCatalog();
    require(catalog.resolveLocation({QStringLiteral("interface-settings"), {}, {}}) ==
                settings::SettingsLocation{QStringLiteral("interface-settings"),
                                           QStringLiteral("general"), {}},
            "page locations must reveal their first section");
    require(catalog.resolveLocation({QStringLiteral("storage-and-privacy"),
                                     QStringLiteral("missing"), QStringLiteral("missing")}) ==
                settings::SettingsLocation{QStringLiteral("storage-and-privacy"),
                                           QStringLiteral("history"), {}},
            "invalid section and item locations must fall back within their page");
    require(catalog.resolveLocation({QStringLiteral("storage-and-privacy"),
                                     QStringLiteral("history"), QStringLiteral("missing")}) ==
                settings::SettingsLocation{QStringLiteral("storage-and-privacy"),
                                           QStringLiteral("history"), {}},
            "invalid item locations must retain their valid section");
    require(catalog.resolveLocation({QStringLiteral("missing"), {}, {}}) ==
                catalog.defaultLocation(),
            "invalid page locations must use the configured default location");
    require(catalog.resolveLocation({QStringLiteral("screenshot-history"), {}, {}}) ==
                settings::SettingsLocation{QStringLiteral("screenshot-history"), {}, {}},
            "custom pages without generated sections must retain their route location");
}

void invalidCatalogReportsAllConformanceErrors() {
    const settings::SettingsCatalog& builtIn = settings::builtInSettingsCatalog();
    QVector<settings::SettingsPageDefinition> pages = builtIn.pages();
    QVector<settings::SettingsNavigationNode> navigation = builtIn.navigation();

    pages[3].route = pages[0].route;
    pages[3].sections[0].items[0].configurationKey =
        QStringLiteral("interface/language");
    pages[3].sections[0].items[1].id = QStringLiteral("interface-theme");
    pages[4].sections[0].items[1].configurationKey = QStringLiteral("missing/key");
    auto& custom = std::get<settings::SettingsCustomDefinition>(
        pages[4].sections[1].items[0].payload);
    custom.renderer = static_cast<settings::SettingsCustomRenderer>(999);
    pages.push_back({QStringLiteral("empty-page"), QStringLiteral("relative-route"),
                     text("Empty Page"), text("Empty page description"), {}});

    auto* group =
        std::get_if<settings::SettingsNavigationGroupDefinition>(&navigation[2]);
    require(group != nullptr, "built-in Settings navigation group must exist");
    group->pages[0].pageId = QStringLiteral("missing-page");

    const settings::SettingsCatalog invalid(
        std::move(pages), std::move(navigation),
        {QStringLiteral("missing-page"), QStringLiteral("missing-section"), {}});
    const QString errors = invalid.validationErrors().join(u'\n');
    require(errors.contains(QStringLiteral("duplicate route")),
            "catalog validation must report route uniqueness errors");
    require(errors.contains(QStringLiteral("generated object name")),
            "catalog validation must report generated object-name collisions");
    require(errors.contains(QStringLiteral("select binding is incompatible")),
            "catalog validation must report binding-to-schema incompatibility");
    require(errors.contains(QStringLiteral("unknown configuration key")),
            "catalog validation must report unknown persistence keys");
    require(errors.contains(QStringLiteral("custom item is incomplete")),
            "catalog validation must report unsupported custom renderers");
    require(errors.contains(QStringLiteral("page has no sections")) &&
                errors.contains(QStringLiteral("page route must be absolute")),
            "catalog validation must report invalid hierarchy metadata");
    require(errors.contains(QStringLiteral("navigation references unknown page")) &&
                errors.contains(QStringLiteral("page is absent from navigation")),
            "catalog validation must report unresolved navigation references");
    require(errors.contains(QStringLiteral("invalid default location")),
            "catalog validation must report an invalid default location");
}

void searchIndexIsGeneratedAndRanked() {
    settings::SettingsSearchIndex index(settings::builtInSettingsCatalog());
    require(index.entries().size() == 22 && index.search(QString()).size() == 22,
            "search must generate all twenty-two catalog nodes in catalog order");

    int pages = 0;
    int sections = 0;
    int items = 0;
    QSet<QString> ids;
    for (const auto& entry : index.entries()) {
        require(!entry.id.isEmpty() && insertUnique(&ids, entry.id),
                "generated search ids must be stable and unique");
        switch (entry.kind) {
        case settings::SettingsSearchNodeKind::Page:
            ++pages;
            require(entry.path == QStringLiteral("Pages"),
                    "page search results must use the Pages path");
            break;
        case settings::SettingsSearchNodeKind::Section:
            ++sections;
            break;
        case settings::SettingsSearchNodeKind::Item:
            ++items;
            break;
        }
    }
    require(pages == 5 && sections == 6 && items == 11,
            "search node counts must match catalog page, section, and item counts");

    const auto theme = index.search(QStringLiteral("theme"));
    require(!theme.isEmpty() && theme.constFirst().id == QStringLiteral("item:interface.theme"),
            "exact item titles must rank ahead of descriptions and paths");
    const auto alias = index.search(QStringLiteral("preferences"));
    require(!alias.isEmpty() &&
                alias.constFirst().location.itemId ==
                    QStringLiteral("quick.open-interface-settings"),
            "configured aliases must be indexed");
    const auto option = index.search(QStringLiteral("dark"));
    require(!option.isEmpty() && option.constFirst().location.itemId ==
                                     QStringLiteral("interface.theme"),
            "select option labels must be indexed");
    const auto multipleTokens = index.search(QStringLiteral("storage error"));
    require(!multipleTokens.isEmpty() &&
                multipleTokens.constFirst().location.itemId == QStringLiteral("storage.status"),
            "every query token must match an indexed field");
    require(index.search(QStringLiteral("storage nonexistent-token")).isEmpty(),
            "a query must be rejected when any token does not match");
}

void searchIndexRebuildsLocalizedFields() {
    settings::SettingsSearchIndex index(settings::builtInSettingsCatalog());
    CatalogTranslator translator;
    require(QCoreApplication::installTranslator(&translator),
            "test translator must install");
    index.rebuild();
    require(!index.search(QStringLiteral("localized theme")).isEmpty() &&
                index.search(QStringLiteral("localized theme")).constFirst().id ==
                    QStringLiteral("item:interface.theme"),
            "search rebuilds must replace localized titles");
    require(!index.search(QStringLiteral("night mode")).isEmpty() &&
                index.search(QStringLiteral("night mode")).constFirst().id ==
                    QStringLiteral("item:interface.theme"),
            "search rebuilds must replace localized select-option labels");
    require(!index.search(QStringLiteral("visual style")).isEmpty() &&
                index.search(QStringLiteral("visual style")).constFirst().id ==
                    QStringLiteral("item:interface.theme"),
            "search rebuilds must replace localized aliases");
    QCoreApplication::removeTranslator(&translator);
    index.rebuild();
    require(index.search(QStringLiteral("localized theme")).isEmpty(),
            "removing a translator must remove stale normalized search fields");
}

void addingCatalogNodesAutomaticallyExpandsSearch() {
    const settings::SettingsCatalog& builtIn = settings::builtInSettingsCatalog();
    QVector<settings::SettingsPageDefinition> pages = builtIn.pages();
    settings::SettingsSelectDefinition select;
    select.options = {
        {QStringLiteral("system"), text("Follow System")},
        {QStringLiteral("light"), text("Light")},
        {QStringLiteral("dark"), text("Dark")},
    };
    pages.push_back({
        QStringLiteral("extra-page"),
        QStringLiteral("/extra"),
        text("Extra Page"),
        text("Extra page description"),
        {{QStringLiteral("extra-section"), text("Extra Section"),
          text("Extra section description"), settings::SettingsSectionReset::None,
          {{QStringLiteral("extra.item"), text("Extra Item"),
            text("Extra item description"), {}, QStringLiteral("interface/theme_mode"),
            select}}}},
    });
    QVector<settings::SettingsNavigationNode> navigation = builtIn.navigation();
    navigation.push_back(settings::SettingsNavigationPageDefinition{
        QStringLiteral("nav.extra-page"), QStringLiteral("extra-page"),
        []() { return adqt::icons::antd::outlined::Appstore(); },
    });
    const settings::SettingsCatalog expanded(std::move(pages), std::move(navigation),
                                             builtIn.defaultLocation());
    require(expanded.validationErrors().isEmpty(),
            "a normal additional catalog page must validate without consumer changes");
    settings::SettingsSearchIndex index(expanded);
    require(index.entries().size() == 25 &&
                index.search(QStringLiteral("extra item")).constFirst().location ==
                    settings::SettingsLocation{QStringLiteral("extra-page"),
                                               QStringLiteral("extra-section"),
                                               QStringLiteral("extra.item")},
            "adding one page, section, and item must automatically add three search entries");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    builtInCatalogIsCompleteAndValid();
    structuredFallbackIsDeterministic();
    invalidCatalogReportsAllConformanceErrors();
    searchIndexIsGeneratedAndRanked();
    searchIndexRebuildsLocalizedFields();
    addingCatalogNodesAutomaticallyExpandsSearch();
    return 0;
}
