#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTURECOORDINATOR_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTURECOORDINATOR_H

#include "snow_shot/presentation/screenshottypes.h"

#include <QObject>
#include <QVector>

class QThread;
class ScreenshotCaptureWorker;

class ScreenshotCaptureCoordinator final : public QObject {
    Q_OBJECT

  public:
    explicit ScreenshotCaptureCoordinator(QObject* parent = nullptr);
    ~ScreenshotCaptureCoordinator() override;

    bool hasWorker() const;
    void ensureWorker();
    void prepareAsync(quint64 requestId);
    void refreshLayoutAsync(quint64 requestId);
    void captureAllAsync(quint64 requestId, bool refreshLayout);
    void releaseIdleResourcesAsync(quint64 requestId);
    void shutdown();

  signals:
    void prepared(quint64 requestId, bool ok);
    void captureFinished(quint64 requestId, QVector<CapturedDisplayModel> displays);

  private:
    template <typename Task> bool postWorkerTask(Task&& task);

    QThread* m_thread = nullptr;
    ScreenshotCaptureWorker* m_worker = nullptr;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCAPTURECOORDINATOR_H
