#include "snow_shot/presentation/screenshotqrrecognitionservice.h"

#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QSize>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>
#include <vector>

namespace {
constexpr qint64 kMaximumDetectorPixels = 1920LL * 1080LL;
constexpr int kMaximumDetectorEdge = 2560;

QString recognitionFailedMessage() {
    return QCoreApplication::translate("ScreenshotOcrController", "QR code recognition failed");
}

QSize boundedDetectorSize(const QSize& sourceSize) {
    if (sourceSize.isEmpty()) {
        return {};
    }

    const double width = sourceSize.width();
    const double height = sourceSize.height();
    const double pixelScale =
        std::sqrt(static_cast<double>(kMaximumDetectorPixels) / (width * height));
    const double edgeScale = static_cast<double>(kMaximumDetectorEdge) / std::max(width, height);
    const double scale = std::min({1.0, pixelScale, edgeScale});
    return QSize(std::max(1, static_cast<int>(std::floor(width * scale))),
                 std::max(1, static_cast<int>(std::floor(height * scale))));
}
} // namespace

class ScreenshotQrRecognitionService::Worker final : public QObject {
  public:
    ScreenshotQrRecognitionResult recognize(QImage source, const std::atomic_bool& cancellation) {
        try {
            const QSize detectorSize = boundedDetectorSize(source.size());
            if (detectorSize.isEmpty() || cancellation.load(std::memory_order_relaxed)) {
                return {};
            }
            if (source.size() != detectorSize) {
                source =
                    source.scaled(detectorSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            if (cancellation.load(std::memory_order_relaxed)) {
                return {};
            }
            if (source.format() != QImage::Format_Grayscale8) {
                source = source.convertToFormat(QImage::Format_Grayscale8);
            }
            if (source.isNull() || cancellation.load(std::memory_order_relaxed)) {
                return {};
            }

            if (m_detector == nullptr) {
                m_detector = std::make_unique<cv::QRCodeDetector>();
            }
            cv::Mat input(source.height(), source.width(), CV_8UC1, source.bits(),
                          static_cast<std::size_t>(source.bytesPerLine()));
            std::vector<std::string> decoded;
            static_cast<void>(m_detector->detectAndDecodeMulti(input, decoded));

            ScreenshotQrRecognitionResult result;
            result.contents.reserve(static_cast<qsizetype>(decoded.size()));
            for (const std::string& value : decoded) {
                if (!value.empty()) {
                    result.contents.push_back(
                        QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
                }
            }
            return result;
        } catch (const cv::Exception& exception) {
            qWarning() << "QR code recognition failed:" << exception.what();
        } catch (const std::exception& exception) {
            qWarning() << "QR code recognition failed:" << exception.what();
        } catch (...) {
            qWarning() << "QR code recognition failed with an unknown error";
        }
        return {{}, recognitionFailedMessage()};
    }

  private:
    std::unique_ptr<cv::QRCodeDetector> m_detector;
};

ScreenshotQrRecognitionService::ScreenshotQrRecognitionService(QObject* parent)
    : ScreenshotQrRecognitionPort(parent), m_workerThread(new QThread(this)), m_worker(new Worker) {
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread->setObjectName(QStringLiteral("ScreenshotQrWorker"));
    m_workerThread->start();
}

ScreenshotQrRecognitionService::~ScreenshotQrRecognitionService() {
    for (const CancellationFlag& cancellation : std::as_const(m_requests)) {
        cancellation->store(true, std::memory_order_relaxed);
    }
    m_workerThread->quit();
    m_workerThread->wait();
    m_worker = nullptr;
}

ScreenshotQrRecognitionPort::RequestToken
ScreenshotQrRecognitionService::recognize(QImage image, QObject* receiver, Completion completion) {
    if (image.isNull() || receiver == nullptr || !completion) {
        return 0;
    }
    const RequestToken token = ++m_nextToken;
    const CancellationFlag cancellation = std::make_shared<std::atomic_bool>(false);
    m_requests.insert(token, cancellation);
    QPointer<ScreenshotQrRecognitionService> service(this);
    m_receiverDestroyedConnections.insert(
        token, connect(receiver, &QObject::destroyed, this, [service, token]() {
            if (service != nullptr) {
                service->cancel(token);
            }
        }));
    QPointer<QObject> guardedReceiver(receiver);
    Worker* worker = m_worker;
    QMetaObject::invokeMethod(
        worker,
        [worker, service, guardedReceiver, token, cancellation, image = std::move(image),
         completion = std::move(completion)]() mutable {
            ScreenshotQrRecognitionResult result;
            if (!cancellation->load(std::memory_order_relaxed)) {
                result = worker->recognize(std::move(image), *cancellation);
            }
            if (service == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                service,
                [service, guardedReceiver, token, completion = std::move(completion),
                 result = std::move(result)]() mutable {
                    if (service != nullptr) {
                        service->deliver(token, guardedReceiver, completion, std::move(result));
                    }
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    return token;
}

void ScreenshotQrRecognitionService::cancel(RequestToken token) {
    const auto request = m_requests.find(token);
    if (request != m_requests.end()) {
        (*request)->store(true, std::memory_order_relaxed);
        m_requests.erase(request);
    }
    QObject::disconnect(m_receiverDestroyedConnections.take(token));
}

void ScreenshotQrRecognitionService::deliver(RequestToken token, const QPointer<QObject>& receiver,
                                             const Completion& completion,
                                             ScreenshotQrRecognitionResult result) {
    const auto request = m_requests.find(token);
    if (request == m_requests.end()) {
        return;
    }
    const CancellationFlag cancellation = *request;
    m_requests.erase(request);
    QObject::disconnect(m_receiverDestroyedConnections.take(token));
    if (cancellation->load(std::memory_order_relaxed) || receiver == nullptr || !completion) {
        return;
    }
    completion(std::move(result));
}
