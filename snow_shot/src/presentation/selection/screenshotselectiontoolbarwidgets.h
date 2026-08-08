#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONTOOLBARWIDGETS_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONTOOLBARWIDGETS_H

#include "icon_core.h"

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class QPainter;

namespace screenshot_selection_toolbar {
inline constexpr int PanelHeight = 26;
inline constexpr int PanelRadius = 6;
inline constexpr int PanelHorizontalPadding = 8;
inline constexpr int PanelVerticalPadding = 2;
inline constexpr int PanelItemSpacing = 0;
inline constexpr int IconSize = 18;
inline constexpr int SymbolHorizontalMargin = 2;
inline constexpr int UnitLeftMargin = 2;
inline constexpr int RadiusShadowSettingGap = 8;
inline constexpr int ShadowMargin = 8;

[[nodiscard]] QColor panelTextColor();
[[nodiscard]] QColor panelPrimaryColor();
[[nodiscard]] bool cursorInsideWidget(const QWidget* widget);
void paintToolbarShadow(QPainter* painter, const QRectF& panelRect, bool hovered);
[[nodiscard]] QPixmap renderToolbarIcon(QWidget* widget, const adqt::icons::IconRef& iconRef,
                                        QColor color);
} // namespace screenshot_selection_toolbar

class SelectionToolbarPanel final : public QFrame {
  public:
    explicit SelectionToolbarPanel(QWidget* parent = nullptr);

  protected:
    void paintEvent(QPaintEvent* event) override;
};

class SelectionToolbarValueLabel final : public QLabel {
  public:
    explicit SelectionToolbarValueLabel(QWidget* parent = nullptr);

    void setLeadingIcon(const QPixmap& icon);
    void setIconOnlyPixmap(const QPixmap& icon);
    void setLockAspectRatioControl(bool enabled);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    QPixmap m_leadingIcon;
    bool m_iconOnly = false;
    bool m_lockAspectRatioControl = false;
};

class SelectionToolbarSeparator final : public QWidget {
  public:
    explicit SelectionToolbarSeparator(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* event) override;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONTOOLBARWIDGETS_H
