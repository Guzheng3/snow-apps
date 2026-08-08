#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTUREWORKER_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTUREWORKER_H

#include "snow_shot/presentation/screenshottypes.h"

#include <QObject>
#include <QPointer>

typedef struct SnowCaptureDesktopSessionImpl SnowCaptureDesktopSession;

class ScreenshotCaptureCoordinator;

class ScreenshotCaptureWorker final : public QObject {
  public:
    ~ScreenshotCaptureWorker() override;

    void prepare(quint64 requestId, const QPointer<ScreenshotCaptureCoordinator>& coordinator);
    void refreshLayout(quint64 requestId);
    void releaseIdleResources(quint64 requestId);
    void captureAll(quint64 requestId, const QPointer<ScreenshotCaptureCoordinator>& coordinator,
                    bool refreshLayout);

  private:
    bool ensureSession();
    bool sessionPrepared() const;
    bool prepareSessionIfNeeded();
    static void postPrepared(quint64 requestId,
                             const QPointer<ScreenshotCaptureCoordinator>& coordinator, bool ok);
    static void postCaptureResult(quint64 requestId,
                                  const QPointer<ScreenshotCaptureCoordinator>& coordinator,
                                  QVector<CapturedDisplayModel> displays);

    SnowCaptureDesktopSession* m_session = nullptr;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTUREWORKER_H
