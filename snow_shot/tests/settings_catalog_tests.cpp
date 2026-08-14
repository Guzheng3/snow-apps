#include "snow_shot/presentation/settings/settingscatalog.h"
#include "snow_shot/presentation/settings/settingssearchindex.h"
#include "snow_shot/presentation/components/icons/snowshoticons.h"
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
    QString translate(const char* context, const char* sourceText, const char*,
                      int) const override {
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
    require(catalog.pages().size() == 7, "catalog must contain seven pages");

    qsizetype sectionCount = 0;
    qsizetype itemCount = 0;
    QSet<QString> objectNames;
    for (const auto& page : catalog.pages()) {
        sectionCount += page.sections.size();
        require(!page.id.isEmpty() && !page.route.isEmpty() && page.title.isValid() &&
                    page.description.isValid(),
                "page metadata must be complete");
        require(insertUnique(&objectNames, settings::generatedObjectName(
                                               QStringLiteral("settings-page"), page.id)),
                "generated page object names must be unique");
        for (const auto& section : page.sections) {
            itemCount += section.items.size();
            require(
                insertUnique(&objectNames, settings::generatedObjectName(
                                               QStringLiteral("settings-section"),
                                               QStringLiteral("%1-%2").arg(page.id, section.id))),
                "generated section object names must be unique");
            for (const auto& item : section.items) {
                require(insertUnique(&objectNames, settings::generatedObjectName(
                                                       QStringLiteral("settings-item"), item.id)),
                        "generated item object names must be unique");
                if (!item.configurationKey.isEmpty()) {
                    require(storage::ConfigurationSchema::entry(item.configurationKey) != nullptr,
                            "catalog persistence keys must resolve through ConfigurationSchema");
                }
            }
        }
    }
    require(sectionCount == 21 && itemCount == 65,
            "catalog must contain the expected twenty-one sections and sixty-five items");
    const auto* functionPage = catalog.page(QStringLiteral("function-settings"));
    const auto* smartSelection =
        catalog.item({QStringLiteral("function-settings"), QStringLiteral("screenshot-settings"),
                      QStringLiteral("screenshot.smart-selection")});
    require(functionPage != nullptr &&
                functionPage->route == QStringLiteral("/settings/functionSettings") &&
                smartSelection != nullptr &&
                smartSelection->configurationKey ==
                    QStringLiteral("screenshot_selection/smart_selection") &&
                std::get<settings::SettingsSwitchDefinition>(smartSelection->payload).binding ==
                    settings::SettingsSwitchBinding::SmartSelection,
            "Function Settings must expose the persisted Smart Selection switch");
    require(functionPage->sections.size() == 6 &&
                functionPage->sections.at(0).id == QStringLiteral("screenshot-settings") &&
                functionPage->sections.at(1).id == QStringLiteral("pin-to-screen-settings") &&
                functionPage->sections.at(2).id == QStringLiteral("drawing-settings") &&
                functionPage->sections.at(3).id == QStringLiteral("screen-recording") &&
                functionPage->sections.at(4).id == QStringLiteral("tray-settings") &&
                functionPage->sections.at(5).id == QStringLiteral("global-hotkeys"),
            "Function Settings must order Pin to Screen, Drawing, and Tray around existing sections");
    const auto* encodingPreset =
        catalog.item({QStringLiteral("function-settings"), QStringLiteral("screen-recording"),
                      QStringLiteral("screen-recording.encoding-preset")});
    require(encodingPreset != nullptr,
            "Function Settings must expose the video encoding preset selector");
    const auto& encodingPresetOptions =
        std::get<settings::SettingsSelectDefinition>(encodingPreset->payload).options;
    require(encodingPresetOptions.size() == 5 &&
                encodingPresetOptions.at(0).value == QStringLiteral("ultrafast") &&
                encodingPresetOptions.at(0).label.translated() == QStringLiteral("Ultra fast") &&
                encodingPresetOptions.at(1).value == QStringLiteral("veryfast") &&
                encodingPresetOptions.at(1).label.translated() == QStringLiteral("Very fast") &&
                encodingPresetOptions.at(2).value == QStringLiteral("medium") &&
                encodingPresetOptions.at(2).label.translated() == QStringLiteral("Medium") &&
                encodingPresetOptions.at(3).value == QStringLiteral("veryslow") &&
                encodingPresetOptions.at(3).label.translated() == QStringLiteral("Very slow") &&
                encodingPresetOptions.at(4).value == QStringLiteral("placebo") &&
                encodingPresetOptions.at(4).label.translated() ==
                    QStringLiteral("Maximum compression"),
            "encoding preset labels must be localized independently of their persisted values");
    require(catalog.item({QStringLiteral("function-settings"),
                          QStringLiteral("pin-to-screen-settings"),
                          QStringLiteral("pin-to-screen.mouse-wheel-zoom-mode")}) != nullptr &&
                catalog.item({QStringLiteral("function-settings"),
                              QStringLiteral("pin-to-screen-settings"),
                              QStringLiteral("pin-to-screen.automatic-text-recognition")}) !=
                    nullptr &&
                catalog.item({QStringLiteral("function-settings"),
                              QStringLiteral("pin-to-screen-settings"),
                              QStringLiteral("pin-to-screen.auto-resize-window")}) != nullptr &&
                catalog.item({QStringLiteral("function-settings"),
                              QStringLiteral("drawing-settings"),
                              QStringLiteral("drawing.quick-selection-disabled-tools")}) !=
                    nullptr &&
                catalog.item({QStringLiteral("function-settings"),
                              QStringLiteral("tray-settings"),
                              QStringLiteral("tray.left-click-action")}) != nullptr,
            "Function Settings must own the moved Pin to Screen, Drawing, and Tray controls");

    const auto* settingsGroup =
        std::get_if<settings::SettingsNavigationGroupDefinition>(&catalog.navigation().at(2));
    require(settingsGroup != nullptr && settingsGroup->pages.size() >= 2 &&
                settingsGroup->pages.at(0).pageId == QStringLiteral("interface-settings") &&
                settingsGroup->pages.at(1).pageId == QStringLiteral("function-settings"),
            "Function Settings must appear below Interface Settings in the Settings navigation");
    require(settingsGroup->title.translated() == QStringLiteral("Settings") &&
                settingsGroup->pages.size() == 5 &&
                settingsGroup->pages.at(2).pageId == QStringLiteral("hotkey-settings") &&
                settingsGroup->pages.constLast().pageId == QStringLiteral("system-settings"),
            "Settings navigation group must expose Hotkey and System Settings");
    require(catalog.page(QStringLiteral("hotkey-settings"))->route ==
                QStringLiteral("/settings/hotKeySettings"),
            "Hotkey Settings must remain reachable below Function Settings");

    const auto* interfacePage = catalog.page(QStringLiteral("interface-settings"));
    require(interfacePage != nullptr && interfacePage->sections.size() == 6 &&
                interfacePage->sections.at(1).id == QStringLiteral("interface-screenshot") &&
                interfacePage->sections.at(2).id == QStringLiteral("toolbar") &&
                interfacePage->sections.at(3).id == QStringLiteral("drawing") &&
                interfacePage->sections.at(4).id == QStringLiteral("pin-to-screen") &&
                interfacePage->sections.at(5).id == QStringLiteral("tray"),
            "Interface Settings must expose Screenshot, Toolbar, Drawing, Pin to Screen, and Tray");
    const auto* toolbarSize =
        catalog.item({QStringLiteral("interface-settings"), QStringLiteral("toolbar"),
                      QStringLiteral("interface.screenshot.toolbar-size")});
    const auto* toolbarEditor =
        catalog.item({QStringLiteral("interface-settings"), QStringLiteral("drawing"),
                      QStringLiteral("interface.toolbar.drawing-toolbar-editor")});
    const auto& toolbarSection = interfacePage->sections.at(2);
    const auto* trayIcon =
        catalog.item({QStringLiteral("interface-settings"), QStringLiteral("tray"),
                      QStringLiteral("interface.tray.icon")});
    require(toolbarSize != nullptr && toolbarEditor != nullptr && trayIcon != nullptr &&
                catalog.item({QStringLiteral("interface-settings"), QStringLiteral("drawing"),
                              QStringLiteral("drawing.quick-selection-disabled-tools")}) ==
                    nullptr &&
                catalog.item({QStringLiteral("interface-settings"),
                              QStringLiteral("pin-to-screen"),
                              QStringLiteral("pin-to-screen.mouse-wheel-zoom-mode")}) ==
                    nullptr &&
                catalog.item({QStringLiteral("interface-settings"), QStringLiteral("tray"),
                              QStringLiteral("tray.left-click-action")}) == nullptr &&
                toolbarSize->configurationKey == QStringLiteral("screenshot_ui/toolbar_size") &&
                toolbarEditor->configurationKey == QStringLiteral("screenshot_toolbar/layout") &&
                toolbarSection.title.translated() == QStringLiteral("Toolbar") &&
                toolbarSection.searchDescription.translated() ==
                    QStringLiteral("Configure the screenshot, pinned, and recording toolbars") &&
                toolbarEditor->title.translated() == QStringLiteral("Drawing toolbar settings") &&
                toolbarEditor->description.translated() ==
                    QStringLiteral("Drag drawing tools to reorder them or stack them in the same "
                                   "toolbar position.") &&
                toolbarEditor->aliases.size() == 2 &&
                toolbarEditor->aliases.at(0).translated() == QStringLiteral("Tool positions") &&
                toolbarEditor->aliases.at(1).translated() ==
                    QStringLiteral("Stack drawing tools") &&
                trayIcon->configurationKey == QStringLiteral("tray/icon") &&
                std::get<settings::SettingsRadioDefinition>(trayIcon->payload).options.size() == 6,
            "new Interface Settings controls must retain their schema contracts");

    const auto* retention =
        storage::ConfigurationSchema::entry(QStringLiteral("capture_history/retention_days"));
    const auto* shortcuts =
        storage::ConfigurationSchema::entry(QStringLiteral("global_shortcuts/screenshot"));
    require(retention != nullptr &&
                retention->valueKind == storage::ConfigurationValueKind::Integer &&
                retention->integerRange.has_value() && retention->integerRange->minimum == 1 &&
                retention->integerRange->maximum == 365,
            "integer schema metadata must expose renderer constraints");
    require(shortcuts != nullptr &&
                shortcuts->valueKind == storage::ConfigurationValueKind::StringList &&
                shortcuts->maximumListItems == 2,
            "shortcut schema metadata must expose list limits");
}

void quickFunctionShortcutsHaveStableContracts() {
    using Action = snow_shot::presentation::GlobalShortcutAction;

    struct ShortcutExpectation {
        Action action;
        const char* sectionId;
        const char* itemId;
        const char* configurationKey;
        settings::SettingsCommandKind commandKind;
        settings::SettingsShortcutAdjustment adjustment =
            settings::SettingsShortcutAdjustment::None;
    };

    const QVector<ShortcutExpectation> expectations{
        {Action::Screenshot, "screenshot", "quick.screenshot", "global_shortcuts/screenshot",
         settings::SettingsCommandKind::CaptureScreenshot},
        {Action::ScreenshotDelay, "screenshot", "quick.screenshot-delay",
         "global_shortcuts/screenshot_delay", settings::SettingsCommandKind::ExecuteQuickAction,
         settings::SettingsShortcutAdjustment::ScreenshotDelaySeconds},
        {Action::ScreenshotFixed, "screenshot", "quick.screenshot-fixed",
         "global_shortcuts/screenshot_fixed", settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenshotOcr, "screenshot", "quick.screenshot-ocr",
         "global_shortcuts/screenshot_ocr", settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenshotCopy, "screenshot", "quick.screenshot-copy",
         "global_shortcuts/screenshot_copy", settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenshotFullScreen, "screenshot", "quick.screenshot-full-screen",
         "global_shortcuts/screenshot_full_screen",
         settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenshotFocusedWindow, "screenshot", "quick.screenshot-focused-window",
         "global_shortcuts/screenshot_focused_window",
         settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenRecord, "screen-recording", "quick.screen-record",
         "global_shortcuts/screen_record", settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ScreenRecordCopy, "screen-recording", "quick.screen-record-copy",
         "global_shortcuts/screen_record_copy", settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::ShowOrHideMainWindow, "other", "quick.show-or-hide-main-window",
         "global_shortcuts/show_or_hide_main_window",
         settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::OpenCaptureHistory, "other", "quick.open-capture-history",
         "global_shortcuts/open_capture_history",
         settings::SettingsCommandKind::ExecuteQuickAction},
        {Action::PinClipboardContent, "other", "quick.pin-clipboard-content",
         "global_shortcuts/pin_clipboard_content",
         settings::SettingsCommandKind::ExecuteQuickAction},
    };

    const settings::SettingsCatalog& catalog = settings::builtInSettingsCatalog();
    QSet<Action> actions;
    for (const ShortcutExpectation& expectation : expectations) {
        const settings::SettingsLocation location{QStringLiteral("quick-functions"),
                                                  QString::fromLatin1(expectation.sectionId),
                                                  QString::fromLatin1(expectation.itemId)};
        const auto* item = catalog.item(location);
        require(item != nullptr, "every quick-function shortcut item must exist");
        require(item->configurationKey == QString::fromLatin1(expectation.configurationKey),
                "quick-function shortcut persistence keys must remain stable");

        const auto* shortcut =
            std::get_if<settings::SettingsShortcutActionDefinition>(&item->payload);
        require(shortcut != nullptr && shortcut->shortcutAction == expectation.action &&
                    shortcut->command.kind == expectation.commandKind &&
                    shortcut->adjustment == expectation.adjustment,
                "quick-function shortcut payloads must match their declared action contracts");
        require(!actions.contains(shortcut->shortcutAction),
                "quick-function shortcut actions must be unique");
        actions.insert(shortcut->shortcutAction);

        const auto command = catalog.commandForShortcut(expectation.action);
        require(command.has_value() && command->kind == expectation.commandKind,
                "every quick-function shortcut must resolve to its configured command");
        if (expectation.commandKind == settings::SettingsCommandKind::CaptureScreenshot ||
            expectation.commandKind == settings::SettingsCommandKind::ExecuteQuickAction) {
            require(shortcut->command.shortcutAction == expectation.action &&
                        command->shortcutAction == expectation.action,
                    "action commands must dispatch their originating shortcut action");
        }
    }

    require(actions.size() == expectations.size() && expectations.size() == 12,
            "the quick-functions catalog must expose all twelve shortcut actions exactly once");
    require(!catalog.commandForShortcut(Action::OpenSettings).has_value(),
            "Open Interface Settings must not appear in Quick Functions");

    const auto* delaySchema =
        storage::ConfigurationSchema::entry(QStringLiteral("screenshot/delay_seconds"));
    require(delaySchema != nullptr &&
                delaySchema->valueKind == storage::ConfigurationValueKind::Integer &&
                delaySchema->defaultValue.toInt() == 3 && delaySchema->integerRange.has_value() &&
                delaySchema->integerRange->minimum == 1 && delaySchema->integerRange->maximum == 10,
            "delayed screenshots must use the persisted 3-second default and 1-10 second range");

    const auto* ocrItem =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("screenshot"),
                      QStringLiteral("quick.screenshot-ocr")});
    const auto* ocrShortcut =
        ocrItem != nullptr
            ? std::get_if<settings::SettingsShortcutActionDefinition>(&ocrItem->payload)
            : nullptr;
    require(ocrShortcut != nullptr && ocrShortcut->iconFactory &&
                ocrShortcut->iconFactory() ==
                    snow_shot::presentation::icons::custom::outlined::ToolRecognizeText(),
            "Text Recognition quick action must use the screenshot toolbar OCR icon");

    const auto* recordingSection =
        catalog.section(QStringLiteral("quick-functions"), QStringLiteral("screen-recording"));
    require(recordingSection != nullptr && recordingSection->title.source != nullptr &&
                QString::fromLatin1(recordingSection->title.source) ==
                    QStringLiteral("Screen Recording") &&
                recordingSection->items.size() == 2,
            "Screen Recording must expose exactly its two quick actions");

    const auto* screenRecord =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("screen-recording"),
                      QStringLiteral("quick.screen-record")});
    const auto* screenRecordCopy =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("screen-recording"),
                      QStringLiteral("quick.screen-record-copy")});
    const auto* showOrHide =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("other"),
                      QStringLiteral("quick.show-or-hide-main-window")});
    const auto* openHistory =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("other"),
                      QStringLiteral("quick.open-capture-history")});
    const auto* pinClipboard =
        catalog.item({QStringLiteral("quick-functions"), QStringLiteral("other"),
                      QStringLiteral("quick.pin-clipboard-content")});
    const auto shortcutPayload = [](const settings::SettingsItemDefinition* item) {
        return item != nullptr
                   ? std::get_if<settings::SettingsShortcutActionDefinition>(&item->payload)
                   : nullptr;
    };
    const auto* screenRecordShortcut = shortcutPayload(screenRecord);
    const auto* screenRecordCopyShortcut = shortcutPayload(screenRecordCopy);
    const auto* showOrHideShortcut = shortcutPayload(showOrHide);
    const auto* openHistoryShortcut = shortcutPayload(openHistory);
    const auto* pinClipboardShortcut = shortcutPayload(pinClipboard);
    require(screenRecord != nullptr && screenRecord->title.source != nullptr &&
                QString::fromLatin1(screenRecord->title.source) ==
                    QStringLiteral("Screen Recording") &&
                screenRecordShortcut != nullptr && screenRecordShortcut->iconFactory &&
                screenRecordShortcut->iconFactory() ==
                    snow_shot::presentation::icons::custom::outlined::RecordScreen(),
            "Screen Recording must use the screenshot toolbar recording icon");
    require(screenRecordCopy != nullptr && screenRecordCopy->title.source != nullptr &&
                QString::fromLatin1(screenRecordCopy->title.source) ==
                    QStringLiteral("Start Recording / Stop Recording and Copy Video") &&
                screenRecordCopyShortcut != nullptr && screenRecordCopyShortcut->iconFactory &&
                screenRecordCopyShortcut->iconFactory() ==
                    snow_shot::presentation::icons::custom::outlined::ScreenshotCopy(),
            "recording toggle must use the exact screenshot-copy icon");
    require(showOrHide != nullptr && showOrHide->title.source != nullptr &&
                QString::fromLatin1(showOrHide->title.source) ==
                    QStringLiteral("Show/Hide Main Window") &&
                showOrHideShortcut != nullptr && showOrHideShortcut->iconFactory &&
                showOrHideShortcut->iconFactory() == adqt::icons::antd::outlined::Appstore(),
            "Show/Hide Main Window must use the Appstore outlined icon");
    require(openHistory != nullptr && openHistory->title.source != nullptr &&
                QString::fromLatin1(openHistory->title.source) ==
                    QStringLiteral("Screenshot History") &&
                openHistoryShortcut != nullptr && openHistoryShortcut->iconFactory &&
                openHistoryShortcut->iconFactory() == adqt::icons::antd::outlined::History(),
            "Screenshot History must use the History outlined icon");
    require(pinClipboard != nullptr && pinClipboard->title.source != nullptr &&
                QString::fromLatin1(pinClipboard->title.source) ==
                    QStringLiteral("Pin Clipboard Content to Screen") &&
                pinClipboardShortcut != nullptr && pinClipboardShortcut->iconFactory &&
                pinClipboardShortcut->iconFactory() ==
                    snow_shot::presentation::icons::custom::outlined::PinToScreen(),
            "clipboard pinning must use the Pin to Screen outlined icon");
}

void structuredFallbackIsDeterministic() {
    const settings::SettingsCatalog& catalog = settings::builtInSettingsCatalog();
    require(catalog.resolveLocation({QStringLiteral("interface-settings"), {}, {}}) ==
                settings::SettingsLocation{
                    QStringLiteral("interface-settings"), QStringLiteral("general"), {}},
            "page locations must reveal their first section");
    require(catalog.resolveLocation({QStringLiteral("storage-and-privacy"),
                                     QStringLiteral("missing"), QStringLiteral("missing")}) ==
                settings::SettingsLocation{
                    QStringLiteral("storage-and-privacy"), QStringLiteral("history"), {}},
            "invalid section and item locations must fall back within their page");
    require(catalog.resolveLocation({QStringLiteral("storage-and-privacy"),
                                     QStringLiteral("history"), QStringLiteral("missing")}) ==
                settings::SettingsLocation{
                    QStringLiteral("storage-and-privacy"), QStringLiteral("history"), {}},
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
    pages[3].sections[0].items[0].configurationKey = QStringLiteral("interface/language");
    pages[3].sections[0].items[1].id = QStringLiteral("interface-theme");
    pages[4].sections[0].items[1].configurationKey = QStringLiteral("missing/key");
    auto& custom =
        std::get<settings::SettingsCustomDefinition>(pages[4].sections[1].items[0].payload);
    custom.renderer = static_cast<settings::SettingsCustomRenderer>(999);
    pages.push_back({QStringLiteral("empty-page"),
                     QStringLiteral("relative-route"),
                     text("Empty Page"),
                     text("Empty page description"),
                     {}});

    auto* group = std::get_if<settings::SettingsNavigationGroupDefinition>(&navigation[2]);
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
    require(index.entries().size() == 93 && index.search(QString()).size() == 93,
            "search must generate all ninety-three catalog nodes in catalog order");

    int pages = 0;
    int sections = 0;
    int items = 0;
    QSet<QString> ids;
    for (const auto& entry : index.entries()) {
        require(!entry.id.isEmpty() && insertUnique(&ids, entry.id),
                "generated search ids must be stable and unique");
        require(!entry.title.contains(QStringLiteral("%1")) &&
                    !entry.description.contains(QStringLiteral("%1")) &&
                    !entry.path.contains(QStringLiteral("%1")),
                "search display fields must never expose unresolved placeholders");
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
    require(pages == 7 && sections == 21 && items == 65,
            "search node counts must match catalog page, section, and item counts");

    const auto theme = index.search(QStringLiteral("theme"));
    require(!theme.isEmpty() && theme.constFirst().id == QStringLiteral("item:interface.theme"),
            "exact item titles must rank ahead of descriptions and paths");
    require(index.search(QStringLiteral("preferences")).isEmpty(),
            "removed quick functions must not remain in search");
    const auto option = index.search(QStringLiteral("dark"));
    require(!option.isEmpty() &&
                option.constFirst().location.itemId == QStringLiteral("interface.theme"),
            "select option labels must be indexed");
    const auto multipleTokens = index.search(QStringLiteral("storage error"));
    require(!multipleTokens.isEmpty() &&
                multipleTokens.constFirst().location.itemId == QStringLiteral("storage.status"),
            "every query token must match an indexed field");
    require(index.search(QStringLiteral("storage nonexistent-token")).isEmpty(),
            "a query must be rejected when any token does not match");
    const auto drawingToolbar = index.search(QStringLiteral("stack drawing tools"));
    require(!drawingToolbar.isEmpty() &&
                drawingToolbar.constFirst().location.itemId ==
                    QStringLiteral("interface.toolbar.drawing-toolbar-editor"),
            "drawing toolbar position and stack terminology must be indexed");

    index.setRuntimeValues({7});
    const auto delayedScreenshot = index.search(QStringLiteral("delay 7s"));
    require(!delayedScreenshot.isEmpty() &&
                delayedScreenshot.constFirst().id ==
                    QStringLiteral("item:quick.screenshot-delay") &&
                delayedScreenshot.constFirst().title == QStringLiteral("Delay 7s to Execute") &&
                !delayedScreenshot.constFirst().title.contains(QStringLiteral("%1")),
            "search must resolve runtime placeholders before indexing and displaying item titles");
}

void searchIndexRebuildsLocalizedFields() {
    settings::SettingsSearchIndex index(settings::builtInSettingsCatalog());
    CatalogTranslator translator;
    require(QCoreApplication::installTranslator(&translator), "test translator must install");
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
        {{QStringLiteral("extra-section"),
          text("Extra Section"),
          text("Extra section description"),
          settings::SettingsSectionReset::None,
          {{QStringLiteral("extra.item"),
            text("Extra Item"),
            text("Extra item description"),
            {},
            QStringLiteral("interface/theme_mode"),
            select}}}},
    });
    QVector<settings::SettingsNavigationNode> navigation = builtIn.navigation();
    navigation.push_back(settings::SettingsNavigationPageDefinition{
        QStringLiteral("nav.extra-page"),
        QStringLiteral("extra-page"),
        []() { return adqt::icons::antd::outlined::Appstore(); },
    });
    const settings::SettingsCatalog expanded(std::move(pages), std::move(navigation),
                                             builtIn.defaultLocation());
    require(expanded.validationErrors().isEmpty(),
            "a normal additional catalog page must validate without consumer changes");
    settings::SettingsSearchIndex index(expanded);
    require(index.entries().size() == 96 &&
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
    quickFunctionShortcutsHaveStableContracts();
    structuredFallbackIsDeterministic();
    invalidCatalogReportsAllConformanceErrors();
    searchIndexIsGeneratedAndRanked();
    searchIndexRebuildsLocalizedFields();
    addingCatalogNodesAutomaticallyExpandsSearch();
    return 0;
}
