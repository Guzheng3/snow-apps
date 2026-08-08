#include "snow_shot/presentation/screenshotqrrecognitionservice.h"

#include <opencv2/core.hpp>
#include <opencv2/wechat_qrcode.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QThread>

#include <exception>
#include <utility>
#include <vector>

namespace {
QString recognitionFailedMessage() {
    return QCoreApplication::translate("ScreenshotOcrController", "QR code recognition failed");
}
} // namespace

class ScreenshotQrRecognitionService::Worker final : public QObject {
  public:
    ScreenshotQrRecognitionResult recognize(QImage source) {
        try {
            if (m_detector == nullptr) {
                m_detector = std::make_unique<cv::wechat_qrcode::WeChatQRCode>();
            }
            if (source.format() != QImage::Format_Grayscale8) {
                source = source.convertToFormat(QImage::Format_Grayscale8);
            }
            cv::Mat input(source.height(), source.width(), CV_8UC1, source.bits(),
                          static_cast<std::size_t>(source.bytesPerLine()));
            std::vector<cv::Mat> points;
            const std::vector<std::string> decoded = m_detector->detectAndDecode(input, points);

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
        }
        return {{}, recognitionFailedMessage()};
    }

  private:
    std::unique_ptr<cv::wechat_qrcode::WeChatQRCode> m_detector;
};

ScreenshotQrRecognitionService::ScreenshotQrRecognitionService(QObject* parent)
    : ScreenshotQrRecognitionPort(parent), m_workerThread(new QThread(this)), m_worker(new Worker) {
    m_worker->moveToThread(m_workerThread);
    m_workerThread->setObjectName(QStringLiteral("ScreenshotQrWorker"));
    m_workerThread->start();
}

ScreenshotQrRecognitionService::~ScreenshotQrRecognitionService() {
    for (const CancellationFlag& cancellation : std::as_const(m_requests)) {
        cancellation->store(true, std::memory_order_relaxed);
    }
    if (m_worker != nullptr) {
        Worker* worker = m_worker;
        QMetaObject::invokeMethod(
            worker, [worker]() { delete worker; }, Qt::BlockingQueuedConnection);
        m_worker = nullptr;
    }
    m_workerThread->quit();
    m_workerThread->wait();
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
    QPointer<QObject> guardedReceiver(receiver);
    Worker* worker = m_worker;
    QMetaObject::invokeMethod(
        worker,
        [worker, service, guardedReceiver, token, cancellation, image = std::move(image),
         completion = std::move(completion)]() mutable {
            ScreenshotQrRecognitionResult result;
            if (!cancellation->load(std::memory_order_relaxed)) {
                result = worker->recognize(std::move(image));
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
    const auto request = m_requests.constFind(token);
    if (request != m_requests.cend()) {
        (*request)->store(true, std::memory_order_relaxed);
    }
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
    if (cancellation->load(std::memory_order_relaxed) || receiver == nullptr || !completion) {
        return;
    }
    completion(std::move(result));
}
