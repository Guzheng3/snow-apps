#ifndef SNOW_SHOT_PRESENTATION_RECORDING_VIDEORECORDINGGEOMETRY_H
#define SNOW_SHOT_PRESENTATION_RECORDING_VIDEORECORDINGGEOMETRY_H

#include <QRect>
#include <QRectF>

namespace snow_shot::presentation::recording {
struct VideoRecordingAreaFrameGeometry {
    QRect windowGeometry;
    QRectF frameRect;
    QRectF selectionRect;
    qreal borderWidth = 1.0;
};

[[nodiscard]] VideoRecordingAreaFrameGeometry
videoRecordingAreaFrameGeometry(const QRectF& logicalRegion, qreal physicalScale);

[[nodiscard]] QRect videoRecordingCompatibleCaptureRegion(const QRect& selectedPhysicalRegion,
                                                          const QRect& physicalBounds);
} // namespace snow_shot::presentation::recording

#endif // SNOW_SHOT_PRESENTATION_RECORDING_VIDEORECORDINGGEOMETRY_H
