#ifndef SNOW_SHOT_PRESENTATION_VIDEORECORDINGTOOLBARWINDOW_H
#define SNOW_SHOT_PRESENTATION_VIDEORECORDINGTOOLBARWINDOW_H

#include "snow_shot/presentation/screenshotfloatingtoolpalettewindow.h"

#include <QRect>

class VideoRecordingToolbarWindow final : public ScreenshotFloatingToolPaletteWindow {
    Q_OBJECT

  public:
    explicit VideoRecordingToolbarWindow(QWidget* parent = nullptr);

    void placeForPhysicalRegion(const QRect& physicalRegion);
};

#endif // SNOW_SHOT_PRESENTATION_VIDEORECORDINGTOOLBARWINDOW_H
