#include "snow_shot/presentation/systemtraycontroller.h"

#include "snow_shot/presentation/languagemanager.h"

#include "widgets/context_menu.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QImageReader>
#include <QPixmap>
#include <QSystemTrayIcon>

namespace snow_shot::presentation {
namespace {
constexpr auto DEFAULT_TRAY_ICON = "default";

const QHash<QString, QString>& bundledIconResources() {
    static const QHash<QString, QString> resources{
        {QStringLiteral("default"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-default.png")},
        {QStringLiteral("light"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-light.png")},
        {QStringLiteral("dark"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-dark.png")},
        {QStringLiteral("snow-default"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-snow-default.png")},
        {QStringLiteral("snow-light"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-snow-light.png")},
        {QStringLiteral("snow-dark"),
         QStringLiteral(":/snow-shot/app-icons/snow-shot-tray-snow-dark.png")},
    };
    return resources;
}

QString normalizedIconSelection(const QString& selection) {
    return bundledIconResources().contains(selection) ? selection
                                                      : QString::fromLatin1(DEFAULT_TRAY_ICON);
}

QString bundledIconResource(const QString& selection) {
    return bundledIconResources().value(normalizedIconSelection(selection));
}

QIcon loadImageIcon(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return {};
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QStringLiteral("png") && suffix != QStringLiteral("ico")) {
        return {};
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    return image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image));
}
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
        trayIcon->setToolTip(QStringLiteral("SnowShot"));
        updateIcon();

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

    void updateIcon() {
        QIcon icon = loadImageIcon(customIconPath);
        QString resolvedSource = customIconPath;
        if (icon.isNull()) {
            resolvedSource = bundledIconResource(iconSelection);
            icon = loadImageIcon(resolvedSource);
        }
        if (icon.isNull()) {
            resolvedSource = QStringLiteral("application-window-icon");
            icon = QApplication::windowIcon();
        }
        if (icon.isNull()) {
            resolvedSource = QCoreApplication::applicationFilePath();
            icon = QIcon(QCoreApplication::applicationFilePath());
        }
        trayIcon->setIcon(icon);
        trayIcon->setProperty("resolvedIconSource", resolvedSource);
    }

    SystemTrayController& q;
    std::unique_ptr<adqt::widgets::AdContextMenu> menu;
    QSystemTrayIcon* trayIcon = nullptr;
    QAction* screenshotAction = nullptr;
    QAction* showMainWindowAction = nullptr;
    QAction* exitAction = nullptr;
    QString iconSelection = QString::fromLatin1(DEFAULT_TRAY_ICON);
    QString customIconPath;
    bool enabled = true;
};

SystemTrayController::SystemTrayController(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this)) {}

SystemTrayController::~SystemTrayController() = default;

void SystemTrayController::show() {
    if (!m_impl->enabled) {
        m_impl->trayIcon->hide();
        return;
    }
    m_impl->updateIcon();
    m_impl->trayIcon->show();
}

void SystemTrayController::hide() {
    m_impl->trayIcon->hide();
}

void SystemTrayController::setEnabled(bool enabled) {
    if (m_impl->enabled == enabled) {
        return;
    }
    m_impl->enabled = enabled;
    if (enabled) {
        show();
    } else {
        hide();
    }
}

bool SystemTrayController::isEnabled() const {
    return m_impl->enabled;
}

void SystemTrayController::setIconSelection(const QString& selection) {
    const QString normalized = normalizedIconSelection(selection);
    if (m_impl->iconSelection == normalized) {
        return;
    }
    m_impl->iconSelection = normalized;
    m_impl->updateIcon();
}

QString SystemTrayController::iconSelection() const {
    return m_impl->iconSelection;
}

void SystemTrayController::setCustomIconPath(const QString& path) {
    if (m_impl->customIconPath == path) {
        return;
    }
    m_impl->customIconPath = path;
    m_impl->updateIcon();
}

QString SystemTrayController::customIconPath() const {
    return m_impl->customIconPath;
}
} // namespace snow_shot::presentation
