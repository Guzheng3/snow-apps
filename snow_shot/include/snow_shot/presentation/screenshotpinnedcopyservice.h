#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDCOPYSERVICE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDCOPYSERVICE_H

#include "snow_shot/presentation/screenshotclipboardservice.h"
#include "snow_shot/presentation/screenshotresultcompositor.h"

#include <QByteArray>
#include <QImage>
#include <QRectF>
#include <QSize>

#include <functional>
#include <memory>

class QObject;
class QThread;

struct ScreenshotPinnedViewportCopyRequest {
    QByteArray documentSession;
    QImage backgroundImage;
    QRectF backgroundCanvasRect;
    QSize contentPixelSize;
    ScreenshotResultStyle resultStyle;
};

class ScreenshotPinnedCopyService final {
  public:
    using Callback = std::function<void(ScreenshotClipboardPayload)>;

    ScreenshotPinnedCopyService();
    ~ScreenshotPinnedCopyService();

    ScreenshotPinnedCopyService(const ScreenshotPinnedCopyService&) = delete;
    ScreenshotPinnedCopyService& operator=(const ScreenshotPinnedCopyService&) = delete;

    [[nodiscard]] bool requestCurrentViewport(ScreenshotPinnedViewportCopyRequest request,
                                              QObject* receiver, Callback callback);
    [[nodiscard]] bool requestOriginalImage(QImage image, QObject* receiver, Callback callback);
    void invalidate();

  private:
    enum class RequestKind {
        None,
        CurrentViewport,
        OriginalImage,
    };

    [[nodiscard]] bool beginRequest(RequestKind kind, quint64* generation);

    std::unique_ptr<QThread> m_thread;
    QObject* m_worker = nullptr;
    QObject* m_completionContext = nullptr;
    quint64 m_generation = 0;
    RequestKind m_activeKind = RequestKind::None;
    bool m_requestInFlight = false;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDCOPYSERVICE_H
