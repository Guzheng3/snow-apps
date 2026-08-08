#include "snow_shot/presentation/styles/floatingcontroltoolbarstyle.h"
#include "snow_shot/presentation/styles/themecolorscheme.h"
#include "snow_shot/presentation/styles/thememanager.h"

#include "widgets/button.h"

#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QSize>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

namespace {
constexpr const char* kThemeConnectionInstalledProperty =
    "snowShotFloatingToolbarThemeConnectionInstalled";
constexpr const char* kPanelScaleProperty = "snowShotFloatingToolbarPanelScale";

int scaledMetric(int value, qreal scale) {
    if (!std::isfinite(scale) || scale <= 0.0) {
        scale = 1.0;
    }
    return std::max(1, qRound(static_cast<qreal>(value) * scale));
}

QColor toolbarSurfaceColor(const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
    return scheme.map.colorBgContainer.isValid() ? scheme.map.colorBgContainer : QColor(Qt::white);
}

QString cssColor(const QColor& color) {
    if (color.alpha() == 255) {
        return color.name(QColor::HexRgb);
    }

    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

void applyPanelBackground(QFrame* panel, qreal scale,
                          const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
    if (panel == nullptr) {
        return;
    }

    panel->setStyleSheet(
        QStringLiteral("QFrame#%1 {"
                       "  background: %2;"
                       "  border: 0px;"
                       "  border-radius: %3px;"
                       "}")
            .arg(panel->objectName())
            .arg(cssColor(toolbarSurfaceColor(scheme)))
            .arg(scaledMetric(
                snow_shot::presentation::styles::FloatingControlToolbarStyle::PanelRadius, scale)));
}
} // namespace

void snow_shot::presentation::styles::applyFloatingControlToolbarPanelStyle(QFrame* panel,
                                                                            qreal scale) {
    if (panel == nullptr) {
        return;
    }

    panel->setAttribute(Qt::WA_StyledBackground, true);
    panel->setProperty(kPanelScaleProperty, scale);
    applyPanelBackground(panel, scale, snow_shot::presentation::styles::generateThemeColorScheme());
    if (!panel->property(kThemeConnectionInstalledProperty).toBool()) {
        panel->setProperty(kThemeConnectionInstalledProperty, true);
        const auto& themeManager = snow_shot::presentation::styles::ThemeManager::instance();
        QObject::connect(
            &themeManager, &snow_shot::presentation::styles::ThemeManager::themeChanged, panel,
            [panel](const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
                const qreal currentScale = panel->property(kPanelScaleProperty).toDouble();
                applyPanelBackground(panel, currentScale, scheme);
            });
    }
    panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(panel->graphicsEffect());
    if (shadow == nullptr) {
        shadow = new QGraphicsDropShadowEffect(panel);
        panel->setGraphicsEffect(shadow);
    }
    shadow->setBlurRadius(scaledMetric(FloatingControlToolbarStyle::ShadowBlurRadius, scale));
    shadow->setOffset(0.0, scaledMetric(FloatingControlToolbarStyle::ShadowOffsetY, scale));
    shadow->setColor(QColor(0, 0, 0, FloatingControlToolbarStyle::ShadowAlpha));
}

void snow_shot::presentation::styles::applyFloatingControlToolbarButtonStyle(
    adqt::widgets::AdButton* button, qreal scale) {
    if (button == nullptr) {
        return;
    }

    const int buttonSize = scaledMetric(FloatingControlToolbarStyle::ButtonSize, scale);
    const int iconSize = scaledMetric(FloatingControlToolbarStyle::IconSize, scale);
    button->setFocusPolicy(Qt::NoFocus);
    button->setShape(adqt::widgets::AdButton::Shape::Rounded);
    button->setSizeClass(adqt::widgets::AdButton::SizeClass::Small);
    button->setFixedSize(buttonSize, buttonSize);
    button->setIconSize(QSize(iconSize, iconSize));
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}
