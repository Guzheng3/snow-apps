#ifndef SNOW_SHOT_PRESENTATION_MAINWINDOW_H
#define SNOW_SHOT_PRESENTATION_MAINWINDOW_H

#include <QByteArray>
#include <QMainWindow>

class QEvent;
class QResizeEvent;
class QWidget;
class SidebarWidget;
class ContentCardWidget;
class MainContentHeaderWidget;
class ScreenshotController;
class TitleBarWidget;
namespace snow_shot::presentation::styles {
struct ThemeColorScheme;
}
namespace snow_shot::presentation {
class GlobalShortcutManager;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(ScreenshotController& screenshotController,
                        snow_shot::presentation::GlobalShortcutManager& globalShortcutManager,
                        QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void showAndActivate();
    void showInterfaceSettings();

  protected:
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif

  private:
    void buildUi();
    void applyTheme(const snow_shot::presentation::styles::ThemeColorScheme& scheme);
    void syncTitleBarBottomShadowGeometry();
    void setupDwmShadow();
    TitleBarWidget* m_titleBar = nullptr;
    SidebarWidget* m_sidebar = nullptr;
    MainContentHeaderWidget* m_contentHeader = nullptr;
    ContentCardWidget* m_contentCard = nullptr;
    ScreenshotController* m_screenshotController = nullptr;
    snow_shot::presentation::GlobalShortcutManager* m_globalShortcutManager = nullptr;
    QWidget* m_titleBarBottomShadow = nullptr;
    bool m_isApplyingTheme = false;
};

#endif // SNOW_SHOT_PRESENTATION_MAINWINDOW_H
