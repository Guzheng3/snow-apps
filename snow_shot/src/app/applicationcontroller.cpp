#include "snow_shot/app/applicationcontroller.h"

#include "snow_shot/presentation/globalshortcutmanager.h"
#include "snow_shot/presentation/mainwindow.h"
#include "snow_shot/presentation/screenshotcontroller.h"
#include "snow_shot/presentation/systemtraycontroller.h"
#include "snow_shot/storage/settingsadapters.h"

#include <QApplication>
#include <QTimer>

#include <memory>

namespace snow_shot::app {
class ApplicationController::Impl {
  public:
    Impl(ApplicationController& owner, QApplication& application) : q(owner), app(application) {
        QObject::connect(&systemTray, &presentation::SystemTrayController::screenshotRequested, &q,
                         [this]() {
                             if (ScreenshotController* controller = ensureScreenshotController()) {
                                 controller->startCapture();
                             }
                         });
        QObject::connect(&systemTray, &presentation::SystemTrayController::showMainWindowRequested,
                         &q, [this]() { showMainWindow(); });
        QObject::connect(&systemTray, &presentation::SystemTrayController::exitRequested, &q,
                         [this]() {
                             systemTray.hide();
                             QApplication::quit();
                         });
        QObject::connect(&globalShortcutManager, &presentation::GlobalShortcutManager::activated,
                         &q, [this](presentation::GlobalShortcutAction action) {
                             dispatchQuickAction(action);
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
        globalShortcutManager.initialize();
        QTimer::singleShot(0, &q, [this]() {
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->prewarmResources();
            }
        });
    }

    ScreenshotController* ensureScreenshotController() {
        if (screenshotController == nullptr) {
            screenshotController = std::make_unique<ScreenshotController>();
        }
        return screenshotController.get();
    }

    MainWindow& ensureMainWindow() {
        if (mainWindow == nullptr) {
            ScreenshotController* controller = ensureScreenshotController();
            Q_ASSERT(controller != nullptr);
            mainWindow = std::make_unique<MainWindow>(*controller, globalShortcutManager);
            QObject::connect(mainWindow.get(), &MainWindow::quickActionRequested, &q,
                             [this](presentation::GlobalShortcutAction action) {
                                 dispatchQuickAction(action);
                             });
        }
        return *mainWindow;
    }

    void dispatchQuickAction(presentation::GlobalShortcutAction action) {
        switch (action) {
        case presentation::GlobalShortcutAction::Screenshot:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->startCapture();
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotDelay:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->startDelayedCapture(
                    storage::ScreenshotSettings().delaySeconds());
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotFixed:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureAndPinSelection();
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotOcr:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureAndRecognizeText();
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotCopy:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureAndCopySelection();
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotFullScreen:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureCurrentMonitor();
            }
            break;
        case presentation::GlobalShortcutAction::ScreenshotFocusedWindow:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureFocusedWindow();
            }
            break;
        case presentation::GlobalShortcutAction::VideoRecord:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->captureAndStartVideoRecording();
            }
            break;
        case presentation::GlobalShortcutAction::VideoRecordCopy:
            if (ScreenshotController* controller = ensureScreenshotController()) {
                controller->startOrStopVideoRecordingAndCopy();
            }
            break;
        case presentation::GlobalShortcutAction::ShowOrHideMainWindow:
            toggleMainWindow();
            break;
        case presentation::GlobalShortcutAction::OpenCaptureHistory:
            ensureMainWindow().showScreenshotHistory();
            break;
        case presentation::GlobalShortcutAction::OpenSettings:
            showInterfaceSettings();
            break;
        }
    }

    void toggleMainWindow() {
        if (mainWindow == nullptr) {
            showMainWindow();
            return;
        }
        // The quick action is intentionally one-way after construction: an
        // existing window is closed regardless of whether it is currently
        // visible, hidden, or minimized.  Showing an existing window belongs
        // to the explicit Show Main Window/tray action.
        mainWindow->close();
    }

    void showMainWindow() {
        ensureMainWindow().showAndActivate();
    }

    void showInterfaceSettings() {
        ensureMainWindow().showInterfaceSettings();
    }

    ApplicationController& q;
    QApplication& app;
    presentation::SystemTrayController systemTray;
    presentation::GlobalShortcutManager globalShortcutManager;
    std::unique_ptr<ScreenshotController> screenshotController;
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
