#include "snow_shot/presentation/systemtraycontroller.h"

#include "snow_shot/presentation/languagemanager.h"
#include "snow_shot/presentation/settings/settingscatalog.h"

#include "widgets/context_menu.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QImageReader>
#include <QPixmap>
#include <QSet>
#include <QSystemTrayIcon>

#include <algorithm>

namespace snow_shot::presentation {
namespace {
constexpr auto DEFAULT_TRAY_ICON = "default";
constexpr auto DEFAULT_LEFT_CLICK_ACTION = "screenshot";

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

QString normalizedLeftClickAction(const QString& action) {
    return action == QStringLiteral("show_main_window")
               ? action
               : QString::fromLatin1(DEFAULT_LEFT_CLICK_ACTION);
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
    Impl(SystemTrayController& owner, const settings::SettingsCatalog& sourceCatalog)
        : q(owner), menu(std::make_unique<adqt::widgets::AdContextMenu>()),
          trayIcon(new QSystemTrayIcon(&owner)), catalog(sourceCatalog),
          groups(catalog.trayMenuGroups()) {
        q.setObjectName(QStringLiteral("systemTrayController"));
        menu->setObjectName(QStringLiteral("systemTrayMenu"));
        menu->setMinimumWidth(300);
        trayIcon->setObjectName(QStringLiteral("snowShotSystemTrayIcon"));
        trayIcon->setToolTip(QStringLiteral("SnowShot"));
        updateIcon();

        buildMenu();
        retranslateUi();
        setMenuOptions({});

        trayIcon->setContextMenu(menu.get());
        QObject::connect(trayIcon, &QSystemTrayIcon::activated, &q,
                         [this](QSystemTrayIcon::ActivationReason reason) {
                             if (reason == QSystemTrayIcon::Trigger) {
                                 if (leftClickAction == QStringLiteral("show_main_window")) {
                                     emit q.showMainWindowRequested();
                                 } else {
                                     emit q.screenshotRequested();
                                 }
                             }
                         });
        QObject::connect(&LanguageManager::instance(), &LanguageManager::languageChanged, &q,
                         [this](const QString&, const QLocale&) { retranslateUi(); });
    }

    ~Impl() {
        trayIcon->hide();
        trayIcon->setContextMenu(nullptr);
    }

    void buildMenu() {
        separatorsBeforeGroup.resize(groups.size());
        for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            const settings::SettingsTrayMenuGroupDefinition& group = groups.at(groupIndex);
            if (groupIndex > 0) {
                QAction* separator = menu->addSeparator();
                separator->setObjectName(
                    QStringLiteral("trayMenuSeparator-%1").arg(group.id));
                separatorsBeforeGroup[groupIndex] = separator;
            }
            for (const settings::SettingsTrayMenuOptionDefinition& option : group.options) {
                const adqt::icons::IconRef icon =
                    option.iconFactory ? option.iconFactory() : adqt::icons::IconRef{};
                QAction* action = menu->addItem(QString(), icon);
                action->setObjectName(
                    settings::generatedObjectName(QStringLiteral("tray-menu-action"), option.id));
                action->setData(option.id);
                actions.insert(option.id, action);
                switch (option.kind) {
                case settings::SettingsTrayMenuOptionKind::QuickAction:
                    QObject::connect(action, &QAction::triggered, &q,
                                     [this, shortcutAction = option.shortcutAction]() {
                                         emit q.quickActionRequested(shortcutAction);
                                     });
                    break;
                case settings::SettingsTrayMenuOptionKind::DisableShortcutFunctions:
                    disableShortcutFunctionsAction = action;
                    action->setCheckable(true);
                    QObject::connect(action, &QAction::toggled, &q,
                                     [this](bool checked) {
                                         emit q.shortcutFunctionsDisabledChanged(checked);
                                     });
                    break;
                case settings::SettingsTrayMenuOptionKind::ShowMainWindow:
                    QObject::connect(action, &QAction::triggered, &q,
                                     &SystemTrayController::showMainWindowRequested);
                    break;
                case settings::SettingsTrayMenuOptionKind::Exit:
                    menu->setActionDanger(action);
                    QObject::connect(action, &QAction::triggered, &q,
                                     &SystemTrayController::exitRequested);
                    break;
                }
            }
        }
    }

    void retranslateUi() {
        for (const settings::SettingsTrayMenuGroupDefinition& group : groups) {
            for (const settings::SettingsTrayMenuOptionDefinition& option : group.options) {
                if (QAction* action = actions.value(option.id)) {
                    const QString label = option.kind == settings::SettingsTrayMenuOptionKind::QuickAction
                                              ? catalog.shortcutActionTitle(option.shortcutAction,
                                                                            screenshotDelaySeconds)
                                              : option.label.translated();
                    Q_ASSERT(!label.isEmpty());
                    action->setText(label);
                }
            }
        }
    }

    void setMenuOptions(const QStringList& options) {
        const QSet<QString> requested(options.cbegin(), options.cend());
        QStringList normalized;
        QVector<bool> visibleGroups(groups.size(), false);
        for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
            for (const settings::SettingsTrayMenuOptionDefinition& option :
                 groups.at(groupIndex).options) {
                QAction* action = actions.value(option.id);
                const bool visible = action != nullptr && requested.contains(option.id);
                if (action != nullptr) {
                    action->setVisible(visible);
                }
                if (visible) {
                    normalized.push_back(option.id);
                    visibleGroups[groupIndex] = true;
                }
            }
        }
        menuOptions = normalized;

        bool priorGroupVisible = visibleGroups.value(0, false);
        for (int groupIndex = 1; groupIndex < groups.size(); ++groupIndex) {
            if (QAction* separator = separatorsBeforeGroup.value(groupIndex)) {
                separator->setVisible(priorGroupVisible && visibleGroups.at(groupIndex));
            }
            priorGroupVisible = priorGroupVisible || visibleGroups.at(groupIndex);
        }

        if (disableShortcutFunctionsAction != nullptr &&
            !disableShortcutFunctionsAction->isVisible() &&
            disableShortcutFunctionsAction->isChecked()) {
            disableShortcutFunctionsAction->setChecked(false);
        }
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
    settings::SettingsCatalog catalog;
    QVector<settings::SettingsTrayMenuGroupDefinition> groups;
    QHash<QString, QAction*> actions;
    QVector<QAction*> separatorsBeforeGroup;
    QAction* disableShortcutFunctionsAction = nullptr;
    QStringList menuOptions;
    QString iconSelection = QString::fromLatin1(DEFAULT_TRAY_ICON);
    QString customIconPath;
    QString leftClickAction = QString::fromLatin1(DEFAULT_LEFT_CLICK_ACTION);
    int screenshotDelaySeconds = 3;
    bool enabled = true;
};

SystemTrayController::SystemTrayController(QObject* parent)
    : SystemTrayController(settings::builtInSettingsCatalog(), parent) {}

SystemTrayController::SystemTrayController(const settings::SettingsCatalog& catalog,
                                           QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this, catalog)) {}

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

void SystemTrayController::setLeftClickAction(const QString& action) {
    m_impl->leftClickAction = normalizedLeftClickAction(action);
}

QString SystemTrayController::leftClickAction() const {
    return m_impl->leftClickAction;
}

void SystemTrayController::setScreenshotDelaySeconds(int seconds) {
    const int normalized = std::clamp(seconds, 1, 10);
    if (m_impl->screenshotDelaySeconds == normalized) {
        return;
    }
    m_impl->screenshotDelaySeconds = normalized;
    m_impl->retranslateUi();
}

int SystemTrayController::screenshotDelaySeconds() const {
    return m_impl->screenshotDelaySeconds;
}

void SystemTrayController::setMenuOptions(const QStringList& options) {
    m_impl->setMenuOptions(options);
}

QStringList SystemTrayController::menuOptions() const {
    return m_impl->menuOptions;
}

bool SystemTrayController::shortcutFunctionsDisabled() const {
    return m_impl->disableShortcutFunctionsAction != nullptr &&
           m_impl->disableShortcutFunctionsAction->isChecked();
}
} // namespace snow_shot::presentation
