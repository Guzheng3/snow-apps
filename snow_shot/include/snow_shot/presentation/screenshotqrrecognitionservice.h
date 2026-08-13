#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTQRRECOGNITIONSERVICE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTQRRECOGNITIONSERVICE_H

#include <QHash>
#include <QImage>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

class QThread;

struct ScreenshotQrRecognitionResult {
    QStringList contents;
    QString error;
};

class ScreenshotQrRecognitionPort : public QObject {
  public:
    explicit ScreenshotQrRecognitionPort(QObject* parent = nullptr) : QObject(parent) {}
    using RequestToken = quint64;
    using Completion = std::function<void(ScreenshotQrRecognitionResult)>;

    ~ScreenshotQrRecognitionPort() override = default;
    virtual RequestToken recognize(QImage image, QObject* receiver, Completion completion) = 0;
    virtual void cancel(RequestToken token) = 0;
};

class ScreenshotQrRecognitionService final : public ScreenshotQrRecognitionPort {
    Q_OBJECT

  public:
    explicit ScreenshotQrRecognitionService(QObject* parent = nullptr);
    ~ScreenshotQrRecognitionService() override;

    RequestToken recognize(QImage image, QObject* receiver, Completion completion) override;
    void cancel(RequestToken token) override;

  private:
    class Worker;

    void deliver(RequestToken token, const QPointer<QObject>& receiver,
                 const Completion& completion, ScreenshotQrRecognitionResult result);

    using CancellationFlag = std::shared_ptr<std::atomic_bool>;

    QThread* m_workerThread = nullptr;
    Worker* m_worker = nullptr;
    QHash<RequestToken, CancellationFlag> m_requests;
    QHash<RequestToken, QMetaObject::Connection> m_receiverDestroyedConnections;
    RequestToken m_nextToken = 0;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTQRRECOGNITIONSERVICE_H
