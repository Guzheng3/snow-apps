#ifndef SNOW_SHOT_PRESENTATION_STYLES_FLOATINGCONTROLTOOLBARSTYLE_H
#define SNOW_SHOT_PRESENTATION_STYLES_FLOATINGCONTROLTOOLBARSTYLE_H

#include <QtGlobal>

class QFrame;

namespace adqt::widgets {
class AdButton;
}

namespace snow_shot::presentation::styles {
struct FloatingControlToolbarStyle {
    static constexpr int ButtonSize = 30;
    static constexpr int IconSize = 18;
    static constexpr int PanelMargin = 5;
    static constexpr int ItemSpacing = 6;
    static constexpr int PanelRadius = 8;
    static constexpr int ShadowBlurRadius = 18;
    static constexpr int ShadowOffsetY = 3;
    static constexpr int ShadowAlpha = 80;
};

void applyFloatingControlToolbarPanelStyle(QFrame* panel, qreal scale = 1.0);
void applyFloatingControlToolbarButtonStyle(adqt::widgets::AdButton* button, qreal scale = 1.0);
} // namespace snow_shot::presentation::styles

#endif // SNOW_SHOT_PRESENTATION_STYLES_FLOATINGCONTROLTOOLBARSTYLE_H
