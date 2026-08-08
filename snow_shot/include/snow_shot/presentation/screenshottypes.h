#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTTYPES_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTTYPES_H

#include <QImage>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QtGlobal>

class QScreen;
class ScreenshotOverlayWindow;

enum class ScreenshotSessionState {
    IdleCold,
    IdlePrepared,
    Capturing,
    OverlayVisible,
    Editing,
    Releasing,
};

enum class ScreenshotOverlayShowMode {
    PreparedPreview,
    CapturedImage,
};

struct ScreenshotDisplayPresentationState {
    ScreenshotOverlayWindow* overlay = nullptr;
};

struct CapturedDisplayModel {
    QString stableId;
    QString name;
    QRect physicalRect;
    QRect canvasRect;
    QRect imageSourceCanvasRect;
    QRect logicalRect;
    QPointer<QScreen> screen;
    QImage image;
    bool active = false;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTTYPES_H
