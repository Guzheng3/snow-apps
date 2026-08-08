#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTDEFAULTSTYLES_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTDEFAULTSTYLES_H

#include "snow_draw_engine_qt/snow_canvas_types.h"

namespace snow_shot::presentation {
inline SnowCanvasStyleDefaults screenshotCanvasStyleDefaults() {
    const QColor red(0xf5, 0x22, 0x2d, 255);
    const QColor redAccent(0xf4, 0x21, 0x2c, 255);
    const QColor transparent(0xff, 0xff, 0xff, 0);

    SnowCanvasStyleDefaults defaults;
    defaults.rectangle.fill = transparent;
    defaults.rectangle.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.rectangle.stroke = red;
    defaults.rectangle.strokeWidth = 2.0;
    defaults.rectangle.strokeStyle = SnowCanvasArrowStrokeStyle::Solid;
    defaults.rectangle.cornerRadii = {6.0, 6.0, 6.0, 6.0};

    defaults.arrow = defaults.rectangle;
    defaults.arrow.startArrowhead = SnowCanvasArrowhead::None;
    defaults.arrow.endArrowhead = SnowCanvasArrowhead::Arrow;
    defaults.arrow.arrowType = SnowCanvasArrowType::Curve;

    defaults.line.fill = transparent;
    defaults.line.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.line.stroke = red;
    defaults.line.strokeWidth = 2.0;
    defaults.line.strokeStyle = SnowCanvasArrowStrokeStyle::Solid;
    defaults.line.startArrowhead = SnowCanvasArrowhead::None;
    defaults.line.endArrowhead = SnowCanvasArrowhead::None;
    defaults.line.opacity = 1.0;

    defaults.freeDraw = defaults.line;
    defaults.freeDraw.stroke = red;

    defaults.rectangleHighlight.fill = red;
    defaults.rectangleHighlight.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.rectangleHighlight.stroke = redAccent;
    defaults.rectangleHighlight.strokeWidth = 0.0;
    defaults.rectangleHighlight.strokeStyle = SnowCanvasArrowStrokeStyle::Solid;
    defaults.rectangleHighlight.opacity = 1.0;
    defaults.rectangleHighlight.highlightShape = SnowCanvasHighlightShape::Rectangle;

    defaults.penHighlight.fill = transparent;
    defaults.penHighlight.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.penHighlight.stroke = red;
    defaults.penHighlight.strokeWidth = 30.0;
    defaults.penHighlight.strokeStyle = SnowCanvasArrowStrokeStyle::Solid;
    defaults.penHighlight.opacity = 1.0;

    defaults.rectangleFilter = {
        SnowCanvasFilterType::Mosaic,
        0.5,
        1.0,
        2.0,
    };
    defaults.penFilter = defaults.rectangleFilter;
    defaults.penFilter.strokeWidth = 30.0;

    defaults.text.color = red;
    defaults.text.fontSize = 30.0;
    defaults.text.fontFamily.clear();
    defaults.text.fill = transparent;
    defaults.text.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.text.stroke = QColor(0xff, 0xcc, 0xc7, 255);
    defaults.text.strokeWidth = 0.0;
    defaults.text.cornerRadii = {6.0, 6.0, 6.0, 6.0};
    defaults.text.horizontalAlign = SnowCanvasTextHorizontalAlign::Left;
    defaults.text.verticalAlign = SnowCanvasTextVerticalAlign::Center;
    defaults.text.opacity = 1.0;

    defaults.serialNumber.number = 1;
    defaults.serialNumber.color = red;
    defaults.serialNumber.fill = transparent;
    defaults.serialNumber.fillStyle = SnowCanvasFillStyle::Solid;
    defaults.serialNumber.fontSize = 24.0;
    defaults.serialNumber.fontFamily.clear();
    defaults.serialNumber.strokeWidth = 2.0;
    defaults.serialNumber.strokeStyle = SnowCanvasStrokeStyle::Solid;
    defaults.serialNumber.opacity = 1.0;

    defaults.watermark.color = QColor(0, 0, 0, 255);
    defaults.watermark.text.clear();
    defaults.watermark.fontSize = 12.0;
    defaults.watermark.fontFamily.clear();
    defaults.watermark.angle = 30.0;
    defaults.watermark.gap = 56.0;
    defaults.watermark.opacity = 0.16;

    defaults.spotlight.color = QColor(0, 0, 0, 255);
    defaults.spotlight.opacity = 0.64;
    return defaults;
}
} // namespace snow_shot::presentation

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTDEFAULTSTYLES_H
