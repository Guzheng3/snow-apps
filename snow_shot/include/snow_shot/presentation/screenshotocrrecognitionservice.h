#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTOCRRECOGNITIONSERVICE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTOCRRECOGNITIONSERVICE_H

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QString>

#include <functional>
#include <memory>

inline constexpr qint64 kScreenshotOcrMaximumPixels = 3840LL * 2160LL;

[[nodiscard]] inline bool screenshotOcrImageWithinPixelLimit(const QSize& size) {
    if (size.width() < 1 || size.height() < 1) {
        return false;
    }
    return static_cast<qint64>(size.width()) * static_cast<qint64>(size.height()) <=
           kScreenshotOcrMaximumPixels;
}

class ScreenshotOcrPresentation;
struct ScreenshotOcrRecognitionResult {
    std::shared_ptr<ScreenshotOcrPresentation> presentation;
    QString error;
};

class ScreenshotOcrRecognitionPort : public QObject {
  public:
    explicit ScreenshotOcrRecognitionPort(QObject* parent = nullptr) : QObject(parent) {}
    using RequestToken = quint64;
    using Completion = std::function<void(ScreenshotOcrRecognitionResult)>;

    virtual ~ScreenshotOcrRecognitionPort() = default;
    virtual RequestToken recognize(QImage image, const QRectF& canvasRect, QObject* receiver,
                                   Completion completion) = 0;
    virtual void cancel(RequestToken token) = 0;
};

class ScreenshotOcrRecognitionService final : public ScreenshotOcrRecognitionPort {
    Q_OBJECT

  public:
    struct Options {
        int engineIdleTimeoutMs = 16'000;
    };

    explicit ScreenshotOcrRecognitionService(QObject* parent = nullptr);
    explicit ScreenshotOcrRecognitionService(std::function<bool()> directMlEnabled,
                                             QObject* parent = nullptr);
    explicit ScreenshotOcrRecognitionService(const Options& options,
                                             std::function<bool()> directMlEnabled = {},
                                             QObject* parent = nullptr);
    ~ScreenshotOcrRecognitionService() override;

    RequestToken recognize(QImage image, const QRectF& canvasRect, QObject* receiver,
                           Completion completion) override;
    void cancel(RequestToken token) override;

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    RequestToken m_nextToken = 0;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTOCRRECOGNITIONSERVICE_H
