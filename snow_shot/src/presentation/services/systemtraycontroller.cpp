#include "snow_shot/presentation/systemtraycontroller.h"

#include "snow_shot/presentation/languagemanager.h"

#include "widgets/context_menu.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QSystemTrayIcon>

namespace snow_shot::presentation {
namespace {
constexpr auto TRAY_ICON_RESOURCE = ":/snow-shot/app-icons/snow-shot-tray-default.png";
} // namespace

class SystemTrayController::Impl {
  public:
    explicit Impl(SystemTrayController& owner)
        : q(owner), menu(std::make_unique<adqt::widgets::AdContextMenu>()),
          trayIcon(new QSystemTrayIcon(&owner)) {
        q.setObjectName(QStringLiteral("systemTrayController"));
        menu->setObjectName(QStringLiteral("systemTrayMenu"));
        menu->setFixedWidth(300);
        trayIcon->setObjectName(QStringLiteral("snowShotSystemTrayIcon"));
        trayIcon->setIcon(QIcon(QString::fromLatin1(TRAY_ICON_RESOURCE)));
        trayIcon->setToolTip(QStringLiteral("SnowShot"));

        screenshotAction = menu->addItem(QString());
        screenshotAction->setObjectName(QStringLiteral("trayScreenshotAction"));
        menu->addSeparator();
        showMainWindowAction = menu->addItem(QString());
        showMainWindowAction->setObjectName(QStringLiteral("trayShowMainWindowAction"));
        exitAction = menu->addItem(QString());
        exitAction->setObjectName(QStringLiteral("trayExitAction"));
        retranslateUi();

        trayIcon->setContextMenu(menu.get());
        QObject::connect(trayIcon, &QSystemTrayIcon::activated, &q,
                         [this](QSystemTrayIcon::ActivationReason reason) {
                             if (reason == QSystemTrayIcon::Trigger) {
                                 emit q.screenshotRequested();
                             }
                         });
        QObject::connect(screenshotAction, &QAction::triggered, &q,
                         &SystemTrayController::screenshotRequested);
        QObject::connect(showMainWindowAction, &QAction::triggered, &q,
                         &SystemTrayController::showMainWindowRequested);
        QObject::connect(exitAction, &QAction::triggered, &q, &SystemTrayController::exitRequested);
        QObject::connect(&LanguageManager::instance(), &LanguageManager::languageChanged, &q,
                         [this](const QString&, const QLocale&) { retranslateUi(); });
    }

    ~Impl() {
        trayIcon->hide();
        trayIcon->setContextMenu(nullptr);
    }

    void retranslateUi() {
        screenshotAction->setText(
            QCoreApplication::translate("SystemTrayController", "Screenshot"));
        showMainWindowAction->setText(
            QCoreApplication::translate("SystemTrayController", "Show Main Window"));
        exitAction->setText(QCoreApplication::translate("SystemTrayController", "Exit"));
    }

    SystemTrayController& q;
    std::unique_ptr<adqt::widgets::AdContextMenu> menu;
    QSystemTrayIcon* trayIcon = nullptr;
    QAction* screenshotAction = nullptr;
    QAction* showMainWindowAction = nullptr;
    QAction* exitAction = nullptr;
};

SystemTrayController::SystemTrayController(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this)) {}

SystemTrayController::~SystemTrayController() = default;

void SystemTrayController::show() {
    if (m_impl->trayIcon->icon().isNull()) {
        QIcon fallbackIcon = QApplication::windowIcon();
        if (fallbackIcon.isNull()) {
            fallbackIcon = QIcon(QCoreApplication::applicationFilePath());
        }
        m_impl->trayIcon->setIcon(fallbackIcon);
    }
    m_impl->trayIcon->show();
}

void SystemTrayController::hide() {
    m_impl->trayIcon->hide();
}
} // namespace snow_shot::presentation
