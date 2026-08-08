#include "snow_shot/app/applicationcontroller.h"

#include "snow_shot/presentation/globalshortcutmanager.h"
#include "snow_shot/presentation/mainwindow.h"
#include "snow_shot/presentation/screenshotcontroller.h"
#include "snow_shot/presentation/systemtraycontroller.h"

#include <QApplication>

#include <memory>

namespace snow_shot::app {
class ApplicationController::Impl {
  public:
    Impl(ApplicationController& owner, QApplication& application) : q(owner), app(application) {
        QObject::connect(&systemTray, &presentation::SystemTrayController::screenshotRequested,
                         &screenshotController, &ScreenshotController::startCapture);
        QObject::connect(&systemTray, &presentation::SystemTrayController::showMainWindowRequested,
                         &q, [this]() { showMainWindow(); });
        QObject::connect(&systemTray, &presentation::SystemTrayController::exitRequested, &q,
                         [this]() {
                             systemTray.hide();
                             QApplication::quit();
                         });
        QObject::connect(&globalShortcutManager, &presentation::GlobalShortcutManager::activated,
                         &q, [this](presentation::GlobalShortcutAction action) {
                             switch (action) {
                             case presentation::GlobalShortcutAction::Screenshot:
                                 screenshotController.startCapture();
                                 break;
                             case presentation::GlobalShortcutAction::OpenSettings:
                                 showInterfaceSettings();
                                 break;
                             }
                         });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &systemTray,
                         &presentation::SystemTrayController::hide);
    }

    void start() {
        if (started) {
            return;
        }
        started = true;

        systemTray.show();
        screenshotController.prewarmResources();
        globalShortcutManager.initialize();
    }

    MainWindow& ensureMainWindow() {
        if (mainWindow == nullptr) {
            mainWindow = std::make_unique<MainWindow>(screenshotController, globalShortcutManager);
        }
        return *mainWindow;
    }

    void showMainWindow() {
        ensureMainWindow().showAndActivate();
    }

    void showInterfaceSettings() {
        ensureMainWindow().showInterfaceSettings();
    }

    ApplicationController& q;
    QApplication& app;
    ScreenshotController screenshotController;
    presentation::GlobalShortcutManager globalShortcutManager;
    presentation::SystemTrayController systemTray;
    std::unique_ptr<MainWindow> mainWindow;
    bool started = false;
};

ApplicationController::ApplicationController(QApplication& application, QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this, application)) {}

ApplicationController::~ApplicationController() = default;

void ApplicationController::start() {
    m_impl->start();
}

void ApplicationController::showMainWindow() {
    m_impl->showMainWindow();
}

void ApplicationController::handleLaunchRequest(const QStringList& arguments) {
    Q_UNUSED(arguments)
    m_impl->showMainWindow();
}
} // namespace snow_shot::app
