#include "snow_shot/presentation/styles/mainwindowcomponenttoken.h"

namespace snow_shot::presentation::styles {
MainWindowComponentMetricToken
buildMainWindowComponentMetricToken(const ThemeColorScheme& colorScheme) {
    const ThemeMetricMapToken& metricMap = colorScheme.metricMap;

    MainWindowComponentMetricToken token;
    token.cardRadius = metricMap.radius.borderRadiusLG + metricMap.radius.borderRadiusXS;
    token.tabGap = metricMap.size.sizeMD;
    token.windowControlMinWidth =
        metricMap.control.controlHeightSM + metricMap.radius.borderRadiusXS;
    token.windowControlMinHeight =
        metricMap.control.controlHeightSM - metricMap.radius.borderRadiusXS;
    token.shortcutKeyMinWidth = metricMap.size.sizeXXL * 3 + metricMap.size.sizeXXS;
    return token;
}
} // namespace snow_shot::presentation::styles
