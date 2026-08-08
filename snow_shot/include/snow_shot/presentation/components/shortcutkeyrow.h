#ifndef SNOW_SHOT_PRESENTATION_COMPONENTS_SHORTCUTKEYROW_H
#define SNOW_SHOT_PRESENTATION_COMPONENTS_SHORTCUTKEYROW_H

#include "icon_core.h"
#include "snow_shot/presentation/globalshortcutmanager.h"
#include "widgets/button.h"

#include <QString>
#include <QStringList>

#include <functional>

#include "snow_shot/presentation/styles/themecolorscheme.h"

class QWidget;
class QLabel;
class QColor;
class QEvent;
class QPaintEvent;
class QObject;
namespace snow_shot::presentation::styles {
struct MainWindowComponentMetricToken;
struct ThemeAliasMetricToken;
} // namespace snow_shot::presentation::styles
struct ShortcutKeyRowConfig {
    QString title;
    adqt::icons::IconRef iconRef;
    QStringList shortcuts;
    snow_shot::presentation::GlobalShortcutRegistrationState registrationState;
    QString rowState;
    bool useStableBorder = false;
    int maxShortcutCount = 2;
    std::function<snow_shot::presentation::GlobalShortcutValidationResult(const QString&)>
        shortcutValidator;
};

class ShortcutKeyRow : public adqt::widgets::AdButton {
    Q_OBJECT

  public:
    explicit ShortcutKeyRow(
        const ShortcutKeyRowConfig& config,
        const snow_shot::presentation::styles::ThemeAliasMetricToken& metric,
        const snow_shot::presentation::styles::MainWindowComponentMetricToken& mainWindowMetric,
        QWidget* parent = nullptr);
    void applyTheme(const snow_shot::presentation::styles::ThemeColorScheme& scheme);
    void setTitle(const QString& title);
    void retranslateUi();
    void
    setRegistrationState(const snow_shot::presentation::GlobalShortcutRegistrationState& state);

  signals:
    void shortcutsChanged(const QStringList& shortcuts);

  protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void openShortcutConfigDialog();
    void syncTitle();
    void syncTitleLabelColor(const QColor& textColor);
    void syncTitleIcon(const QColor& iconColor);
    void syncRegistrationStatus();
    [[nodiscard]] QString registrationTooltipText() const;
    bool isShortcutButtonActive() const;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_titleIcon = nullptr;
    adqt::widgets::AdButton* m_shortcutButton = nullptr;
    QString m_rowState;
    snow_shot::presentation::GlobalShortcutRegistrationState m_registrationState;
    adqt::icons::IconRef m_titleIconRef;
    int m_titleIconSize = 0;
    int m_rowBorderWidth = 1;
    int m_rowBorderRadius = 0;
    bool m_useStableBorder = false;
    int m_maxShortcutCount = 2;
    std::function<snow_shot::presentation::GlobalShortcutValidationResult(const QString&)>
        m_shortcutValidator;
    snow_shot::presentation::styles::ThemeColorScheme m_colorScheme;
};

#endif // SNOW_SHOT_PRESENTATION_COMPONENTS_SHORTCUTKEYROW_H
