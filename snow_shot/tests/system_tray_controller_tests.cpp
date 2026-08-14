#include "snow_shot/presentation/languagemanager.h"
#include "snow_shot/presentation/systemtraycontroller.h"
#include "snow_shot/storage/applicationstorage.h"

#include "widgets/context_menu.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QString>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void requireActionText(const QAction* action, const QString& expected, const char* message) {
    require(action != nullptr && action->text() == expected, message);
}

} // namespace

int main(int argc, char* argv[]) {
    const QString applicationName =
        QStringLiteral("system-tray-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QCoreApplication::setOrganizationName(QStringLiteral("SnowShotTests"));
    QCoreApplication::setApplicationName(applicationName);

    QApplication application(argc, argv);
    QTemporaryDir storageDirectory;
    require(storageDirectory.isValid(), "temporary storage directory should be available");
    static_cast<void>(snow_shot::storage::ApplicationStorage::instance().initialize(
        {storageDirectory.path(), storageDirectory.path(), 8000}));
    auto& languageManager = snow_shot::presentation::LanguageManager::instance();
    require(languageManager.setLanguage(QStringLiteral("en_US")),
            "English should be available from the English catalog");

    snow_shot::presentation::SystemTrayController controller;
    auto* trayIcon =
        controller.findChild<QSystemTrayIcon*>(QStringLiteral("snowShotSystemTrayIcon"));
    require(trayIcon != nullptr, "the controller should own a system tray icon");
    require(!trayIcon->icon().isNull(), "the bundled tray icon should load");
    require(trayIcon->toolTip() == QStringLiteral("SnowShot"),
            "the tray tooltip should be SnowShot");
    controller.show();
    require(trayIcon->isVisible(), "show should make the tray icon visible");
    controller.setEnabled(false);
    require(!controller.isEnabled() && !trayIcon->isVisible(),
            "disabling the tray should hide it immediately");
    controller.show();
    require(!trayIcon->isVisible(), "show should not bypass a disabled tray");
    controller.setEnabled(true);
    require(controller.isEnabled() && trayIcon->isVisible(),
            "enabling the tray should show it immediately");
    controller.hide();
    require(!trayIcon->isVisible(), "hide should make the tray icon invisible");

    const QStringList bundledSelections{
        QStringLiteral("default"),      QStringLiteral("light"),
        QStringLiteral("dark"),         QStringLiteral("snow-default"),
        QStringLiteral("snow-light"),   QStringLiteral("snow-dark"),
    };
    for (const QString& selection : bundledSelections) {
        controller.setIconSelection(selection);
        require(controller.iconSelection() == selection,
                "each supported tray icon selection should be retained");
        require(trayIcon->property("resolvedIconSource").toString() ==
                    QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-%1.png")
                        .arg(selection),
                "each tray icon selection should resolve to its bundled asset");
    }
    controller.setIconSelection(QStringLiteral("unsupported"));
    require(controller.iconSelection() == QStringLiteral("default") &&
                trayIcon->property("resolvedIconSource").toString() ==
                    QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-default.png"),
            "an unsupported tray icon selection should use the bundled default");

    const QString customIconPath = storageDirectory.filePath(QStringLiteral("custom-icon.png"));
    QImage customImage(64, 64, QImage::Format_ARGB32_Premultiplied);
    customImage.fill(QColor(242, 17, 137));
    require(customImage.save(customIconPath), "the custom tray icon fixture should be writable");
    controller.setCustomIconPath(customIconPath);
    require(controller.customIconPath() == customIconPath &&
                trayIcon->icon().pixmap(QSize(64, 64)).toImage().pixelColor(32, 32) ==
                    QColor(242, 17, 137),
            "a valid custom image should replace the bundled tray icon");
    controller.setIconSelection(QStringLiteral("light"));
    controller.setCustomIconPath(storageDirectory.filePath(QStringLiteral("missing.png")));
    require(trayIcon->property("resolvedIconSource").toString() ==
                QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-light.png"),
            "an invalid custom image should fall back to the selected bundled tray icon");
    const QString malformedIconPath =
        storageDirectory.filePath(QStringLiteral("malformed-icon.png"));
    QFile malformedIcon(malformedIconPath);
    require(malformedIcon.open(QIODevice::WriteOnly),
            "the malformed custom icon fixture should be writable");
    malformedIcon.write("not an image");
    malformedIcon.close();
    controller.setCustomIconPath(malformedIconPath);
    require(trayIcon->property("resolvedIconSource").toString() ==
                QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-light.png"),
            "a malformed custom image should fall back to the selected bundled tray icon");
    const QString unsupportedIconPath =
        storageDirectory.filePath(QStringLiteral("unsupported-icon.bmp"));
    require(customImage.save(unsupportedIconPath, "BMP"),
            "the unsupported custom icon fixture should be writable");
    controller.setCustomIconPath(unsupportedIconPath);
    require(trayIcon->property("resolvedIconSource").toString() ==
                QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-light.png"),
            "a readable custom image outside PNG and ICO should use the bundled fallback");

    auto* menu = dynamic_cast<adqt::widgets::AdContextMenu*>(trayIcon->contextMenu());
    require(menu != nullptr, "the tray should use the Ant Design context menu");
    require(menu->minimumWidth() == 300 && menu->maximumWidth() == 300,
            "tray context menu should have a fixed width of 300");
    const QList<QAction*> actions = menu->actions();
    require(actions.size() == 4, "the tray menu should contain three actions and one separator");
    requireActionText(actions[0], QStringLiteral("Screenshot"), "Screenshot should be first");
    require(actions[1]->isSeparator(), "the second menu entry should be a separator");
    requireActionText(actions[2], QStringLiteral("Show Main Window"),
                      "Show Main Window should follow the separator");
    requireActionText(actions[3], QStringLiteral("Exit"), "Exit should be last");

    int screenshotRequests = 0;
    int showMainWindowRequests = 0;
    int exitRequests = 0;
    QObject::connect(&controller,
                     &snow_shot::presentation::SystemTrayController::screenshotRequested,
                     [&screenshotRequests]() { ++screenshotRequests; });
    QObject::connect(&controller,
                     &snow_shot::presentation::SystemTrayController::showMainWindowRequested,
                     [&showMainWindowRequests]() { ++showMainWindowRequests; });
    QObject::connect(&controller, &snow_shot::presentation::SystemTrayController::exitRequested,
                     [&exitRequests]() { ++exitRequests; });

    trayIcon->activated(QSystemTrayIcon::Trigger);
    trayIcon->activated(QSystemTrayIcon::Context);
    trayIcon->activated(QSystemTrayIcon::DoubleClick);
    trayIcon->activated(QSystemTrayIcon::MiddleClick);
    trayIcon->activated(QSystemTrayIcon::Unknown);
    require(screenshotRequests == 1, "only a left-click trigger should request a screenshot");

    controller.setLeftClickAction(QStringLiteral("show_main_window"));
    require(controller.leftClickAction() == QStringLiteral("show_main_window"),
            "the configured show-window left-click action should be retained");
    trayIcon->activated(QSystemTrayIcon::Trigger);
    require(screenshotRequests == 1 && showMainWindowRequests == 1,
            "the configured tray left-click should request the main window");
    controller.setLeftClickAction(QStringLiteral("unsupported"));
    require(controller.leftClickAction() == QStringLiteral("screenshot"),
            "an unsupported tray left-click action should fall back to Screenshot");

    actions[0]->trigger();
    actions[2]->trigger();
    actions[3]->trigger();
    require(screenshotRequests == 2, "the Screenshot menu action should request a screenshot");
    require(showMainWindowRequests == 2, "the Show Main Window action should emit its request");
    require(exitRequests == 1, "the Exit action should emit its request");

    require(languageManager.setLanguage(QStringLiteral("zh_CN")),
            "the Simplified Chinese translation should load");
    requireActionText(actions[0], QStringLiteral("\u5c4f\u5e55\u622a\u56fe"),
                      "Screenshot should translate to Simplified Chinese");
    requireActionText(actions[2], QStringLiteral("\u663e\u793a\u4e3b\u7a97\u53e3"),
                      "Show Main Window should translate to Simplified Chinese");
    requireActionText(actions[3], QStringLiteral("\u9000\u51fa"),
                      "Exit should translate to Simplified Chinese");

    require(languageManager.setLanguage(QStringLiteral("zh_TW")),
            "the Traditional Chinese translation should load");
    requireActionText(actions[0], QStringLiteral("\u87a2\u5e55\u622a\u5716"),
                      "Screenshot should translate to Traditional Chinese");
    requireActionText(actions[2], QStringLiteral("\u986f\u793a\u4e3b\u8996\u7a97"),
                      "Show Main Window should translate to Traditional Chinese");
    requireActionText(actions[3], QStringLiteral("\u7d50\u675f"),
                      "Exit should translate to Traditional Chinese");

    snow_shot::storage::ApplicationStorage::instance().shutdown();
    return 0;
}
