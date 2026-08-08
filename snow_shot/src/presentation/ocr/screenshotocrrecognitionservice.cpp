#include "snow_shot/presentation/screenshotocrrecognitionservice.h"

#include "snow_shot/presentation/screenshotocrpresentation.h"

#include "snow_ocr_c.h"

#include <QMetaObject>
#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
QPolygonF quadFromFfi(const SnowOcrQuad& quad, const QRectF& canvasRect, const QSize& imageSize) {
    const qreal scaleX = imageSize.width() > 0 ? canvasRect.width() / imageSize.width() : 1.0;
    const qreal scaleY = imageSize.height() > 0 ? canvasRect.height() / imageSize.height() : 1.0;
    QPolygonF polygon;
    polygon.reserve(4);
    for (int index = 0; index < 4; ++index) {
        polygon.push_back(
            QPointF(canvasRect.left() + static_cast<qreal>(quad.points[index * 2]) * scaleX,
                    canvasRect.top() + static_cast<qreal>(quad.points[index * 2 + 1]) * scaleY));
    }
    return polygon;
}

qreal edgeLength(const QPointF& first, const QPointF& second) {
    return std::hypot(second.x() - first.x(), second.y() - first.y());
}

ScreenshotOcrTextDirection textDirectionForQuad(const QPolygonF& quad) {
    constexpr qreal kVerticalAspectRatio = 1.5;
    if (quad.size() != 4) {
        return ScreenshotOcrTextDirection::Horizontal;
    }
    const qreal width =
        std::max(edgeLength(quad.at(0), quad.at(1)), edgeLength(quad.at(3), quad.at(2)));
    const qreal height =
        std::max(edgeLength(quad.at(0), quad.at(3)), edgeLength(quad.at(1), quad.at(2)));
    return height >= width * kVerticalAspectRatio ? ScreenshotOcrTextDirection::Vertical
                                                  : ScreenshotOcrTextDirection::Horizontal;
}

QString lastOcrError() {
    const char* message = snow_ocr_last_error_message();
    return message == nullptr
               ? QCoreApplication::translate("ScreenshotOcrController", "Text recognition failed")
               : QString::fromUtf8(message);
}
} // namespace

class ScreenshotOcrRecognitionService::Worker final : public QObject {
  public:
    ~Worker() override {
        snow_ocr_engine_destroy(m_engine);
    }

    ScreenshotOcrRecognitionResult recognize(QImage source, const QRectF& canvasRect) {
        if (m_engine == nullptr) {
            m_engine = snow_ocr_engine_create();
            if (m_engine == nullptr) {
                return {nullptr, lastOcrError()};
            }
        }
        if (source.format() != QImage::Format_RGBA8888) {
            source = source.convertToFormat(QImage::Format_RGBA8888);
        }
        const SnowOcrRequestV1 request{
            static_cast<std::uint32_t>(sizeof(SnowOcrRequestV1)),
            static_cast<std::uint32_t>(source.width()),
            static_cast<std::uint32_t>(source.height()),
            static_cast<std::uint32_t>(source.bytesPerLine()),
            source.constBits(),
            static_cast<std::size_t>(source.sizeInBytes()),
        };
        SnowOcrResult* result = snow_ocr_engine_recognize_rgba(m_engine, &request);
        if (result == nullptr) {
            return {nullptr, lastOcrError()};
        }

        ScreenshotOcrRecognitionResult output;
        output.presentation = std::make_shared<ScreenshotOcrPresentation>();
        output.presentation->selection = canvasRect.toAlignedRect();
        SnowOcrImageInfoV1 imageInfo{};
        imageInfo.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrImageInfoV1));
        if (snow_ocr_result_image(result, &imageInfo) == 0) {
            output.error = lastOcrError();
            snow_ocr_result_destroy(result);
            output.presentation.reset();
            return output;
        }
        output.presentation->filledImage =
            QImage(imageInfo.rgba_bytes, static_cast<int>(imageInfo.width),
                   static_cast<int>(imageInfo.height),
                   static_cast<qsizetype>(imageInfo.stride_bytes), QImage::Format_RGBA8888)
                .copy();

        const std::size_t lineCount = snow_ocr_result_line_count(result);
        output.presentation->lines.reserve(static_cast<qsizetype>(lineCount));
        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
            SnowOcrLineInfoV1 lineInfo{};
            lineInfo.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrLineInfoV1));
            if (snow_ocr_result_line(result, lineIndex, &lineInfo) == 0) {
                output.error = lastOcrError();
                break;
            }
            ScreenshotOcrLine line;
            line.text = QString::fromUtf8(reinterpret_cast<const char*>(lineInfo.text_utf8),
                                          static_cast<qsizetype>(lineInfo.text_len));
            line.confidence = static_cast<qreal>(lineInfo.confidence);
            line.foreground = QColor(lineInfo.foreground.red, lineInfo.foreground.green,
                                     lineInfo.foreground.blue, lineInfo.foreground.alpha);
            line.quad = quadFromFfi(lineInfo.quad, canvasRect, source.size());
            line.direction = textDirectionForQuad(line.quad);
            output.presentation->lines.push_back(std::move(line));
        }
        snow_ocr_result_destroy(result);
        if (!output.error.isEmpty()) {
            output.presentation.reset();
            return output;
        }
        output.presentation->prepareForRendering();
        return output;
    }

  private:
    SnowOcrEngine* m_engine = nullptr;
};

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(QObject* parent)
    : ScreenshotOcrRecognitionPort(parent), m_workerThread(new QThread(this)),
      m_worker(new Worker) {
    m_worker->moveToThread(m_workerThread);
    m_workerThread->setObjectName(QStringLiteral("ScreenshotOcrWorker"));
    m_workerThread->start();
}

ScreenshotOcrRecognitionService::~ScreenshotOcrRecognitionService() {
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

ScreenshotOcrRecognitionPort::RequestToken
ScreenshotOcrRecognitionService::recognize(QImage image, const QRectF& canvasRect,
                                           QObject* receiver, Completion completion) {
    if (image.isNull() || !canvasRect.isValid() || canvasRect.isEmpty() || receiver == nullptr ||
        !completion) {
        return 0;
    }
    const RequestToken token = ++m_nextToken;
    const CancellationFlag cancellation = std::make_shared<std::atomic_bool>(false);
    m_requests.insert(token, cancellation);
    QPointer<ScreenshotOcrRecognitionService> service(this);
    QPointer<QObject> guardedReceiver(receiver);
    Worker* worker = m_worker;
    QMetaObject::invokeMethod(
        worker,
        [worker, service, guardedReceiver, token, cancellation, image = std::move(image), canvasRect,
         completion = std::move(completion)]() mutable {
            ScreenshotOcrRecognitionResult result;
            if (!cancellation->load(std::memory_order_relaxed)) {
                result = worker->recognize(std::move(image), canvasRect);
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

void ScreenshotOcrRecognitionService::cancel(RequestToken token) {
    const auto request = m_requests.constFind(token);
    if (request != m_requests.cend()) {
        (*request)->store(true, std::memory_order_relaxed);
    }
}

void ScreenshotOcrRecognitionService::deliver(RequestToken token, const QPointer<QObject>& receiver,
                                               const Completion& completion,
                                               ScreenshotOcrRecognitionResult result) {
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
