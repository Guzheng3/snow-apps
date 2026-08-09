#include "snow_shot/presentation/screenshotocrrecognitionservice.h"

#include "snow_shot/presentation/screenshotocrpresentation.h"

#include "snow_ocr_c.h"

#include <QCoreApplication>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QTransform>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <utility>

namespace {
int workerIdleTimeoutMs() {
    bool valid = false;
    const int configured = qEnvironmentVariableIntValue("SNOW_SHOT_OCR_IDLE_TIMEOUT_MS", &valid);
    return valid && configured > 0
               ? configured
               : ScreenshotOcrRecognitionService::Options{}.engineIdleTimeoutMs;
}

struct OcrEngineDeleter {
    void operator()(SnowOcrEngine* engine) const {
        snow_ocr_engine_destroy(engine);
    }
};

struct OcrResultDeleter {
    void operator()(SnowOcrResult* result) const {
        snow_ocr_result_destroy(result);
    }
};

struct OcrOwnedImageDeleter {
    void operator()(SnowOcrOwnedImage* image) const {
        snow_ocr_owned_image_destroy(image);
    }
};

using OcrEngineHandle = std::unique_ptr<SnowOcrEngine, OcrEngineDeleter>;
using OcrResultHandle = std::unique_ptr<SnowOcrResult, OcrResultDeleter>;
using OcrOwnedImageHandle = std::unique_ptr<SnowOcrOwnedImage, OcrOwnedImageDeleter>;

void releaseOwnedImage(void* context) {
    snow_ocr_owned_image_destroy(static_cast<SnowOcrOwnedImage*>(context));
}

QString lastOcrError() {
    const char* message = snow_ocr_last_error_message();
    if (message == nullptr || *message == '\0') {
        return QCoreApplication::translate("ScreenshotOcrController", "Text recognition failed");
    }
    return QString::fromUtf8(message);
}

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

bool validOwnedImageInfo(const SnowOcrImageInfoV1& info) {
    if (info.rgba_bytes == nullptr || info.width == 0 || info.height == 0 ||
        info.width > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
        info.height > static_cast<std::uint32_t>((std::numeric_limits<int>::max)()) ||
        info.stride_bytes > static_cast<std::uint32_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const std::uint64_t rowBytes = static_cast<std::uint64_t>(info.width) * 4;
    const std::uint64_t required = static_cast<std::uint64_t>(info.stride_bytes) * info.height;
    return info.stride_bytes >= rowBytes && required <= info.rgba_len;
}

class OcrWorker final : public QObject {
  public:
    OcrWorker()
        : m_idleTimer(new QTimer(this)) {
        m_idleTimer->setSingleShot(true);
        QObject::connect(m_idleTimer, &QTimer::timeout, this,
                         [this]() { m_engine.reset(); });
    }

    ScreenshotOcrRecognitionResult recognize(QImage source, const QRectF& canvasRect,
                                             bool directMlEnabled) {
        m_idleTimer->stop();

        if (!ensureEngine(directMlEnabled)) {
            return {nullptr, lastOcrError()};
        }
        if (source.format() != QImage::Format_RGBA8888) {
            source = source.convertToFormat(QImage::Format_RGBA8888);
        }
        if (source.isNull()) {
            return {nullptr, QCoreApplication::translate("ScreenshotOcrController",
                                                         "Text recognition failed")};
        }

        const SnowOcrRequestV1 request{
            static_cast<std::uint32_t>(sizeof(SnowOcrRequestV1)),
            static_cast<std::uint32_t>(source.width()),
            static_cast<std::uint32_t>(source.height()),
            static_cast<std::uint32_t>(source.bytesPerLine()),
            source.constBits(),
            static_cast<std::size_t>(source.sizeInBytes()),
        };
        OcrResultHandle result(snow_ocr_engine_recognize_rgba(m_engine.get(), &request));
        if (result == nullptr) {
            return {nullptr, lastOcrError()};
        }

        ScreenshotOcrRecognitionResult output;
        output.presentation = std::make_shared<ScreenshotOcrPresentation>();
        output.presentation->selection = canvasRect.toAlignedRect();
        const std::size_t lineCount = snow_ocr_result_line_count(result.get());
        if (lineCount > static_cast<std::size_t>((std::numeric_limits<qsizetype>::max)())) {
            return {nullptr, QCoreApplication::translate("ScreenshotOcrController",
                                                        "Text recognition failed")};
        }
        output.presentation->lines.reserve(static_cast<qsizetype>(lineCount));
        for (std::size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
            SnowOcrLineInfoV1 lineInfo{};
            lineInfo.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrLineInfoV1));
            if (snow_ocr_result_line(result.get(), lineIndex, &lineInfo) == 0 ||
                (lineInfo.text_len > 0 && lineInfo.text_utf8 == nullptr) ||
                lineInfo.text_len >
                    static_cast<std::size_t>((std::numeric_limits<qsizetype>::max)())) {
                return {nullptr, lastOcrError()};
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

        OcrOwnedImageHandle image(snow_ocr_result_take_image(result.get()));
        if (image == nullptr) {
            return {nullptr, lastOcrError()};
        }
        SnowOcrImageInfoV1 imageInfo{};
        imageInfo.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrImageInfoV1));
        if (snow_ocr_owned_image_info(image.get(), &imageInfo) == 0 ||
            !validOwnedImageInfo(imageInfo)) {
            return {nullptr, lastOcrError()};
        }

        SnowOcrOwnedImage* imageContext = image.release();
        QImage filledImage(imageInfo.rgba_bytes, static_cast<int>(imageInfo.width),
                           static_cast<int>(imageInfo.height),
                           static_cast<qsizetype>(imageInfo.stride_bytes),
                           QImage::Format_RGBA8888, &releaseOwnedImage, imageContext);
        if (filledImage.isNull()) {
            snow_ocr_owned_image_destroy(imageContext);
            return {nullptr, QCoreApplication::translate("ScreenshotOcrController",
                                                         "Text recognition failed")};
        }
        output.presentation->filledImage = std::move(filledImage);
        output.presentation->prepareForRendering();
        return output;
    }

    void startIdleTimer(int timeoutMs) {
        if (m_engine != nullptr) {
            m_idleTimer->start(timeoutMs);
        }
    }

    void shutdown() {
        m_idleTimer->stop();
        m_engine.reset();
    }

  private:
    bool ensureEngine(bool directMlEnabled) {
        if (m_engine != nullptr && m_engineDirectMl == directMlEnabled) {
            return true;
        }
        m_engine.reset();
        SnowOcrRuntimeInfoV1 runtimeInfo{
            static_cast<std::uint32_t>(sizeof(SnowOcrRuntimeInfoV1)), 0};
        const std::uint32_t physicalCores =
            snow_ocr_runtime_info_v1(&runtimeInfo) != 0 ? runtimeInfo.physical_core_count : 1;
        const std::uint32_t threadBudget = (std::max)(1u, physicalCores / 2u);
        const SnowOcrEngineConfigV2 config{
            static_cast<std::uint32_t>(sizeof(SnowOcrEngineConfigV2)),
            directMlEnabled ? 1u : threadBudget,
            1u,
            threadBudget,
            1,
            static_cast<std::uint8_t>(directMlEnabled ? 1 : 0),
            {0, 0},
        };
        m_engine.reset(snow_ocr_engine_create_with_config_v2(&config));
        if (m_engine != nullptr) {
            m_engineDirectMl = directMlEnabled;
        }
        return m_engine != nullptr;
    }

    OcrEngineHandle m_engine;
    bool m_engineDirectMl = false;
    QTimer* m_idleTimer = nullptr;
};
} // namespace

class ScreenshotOcrRecognitionService::Impl final {
  public:
    Impl(ScreenshotOcrRecognitionService* owner, const int idleTimeoutMs,
         std::function<bool()> directMlEnabled)
        : m_owner(owner), m_directMlEnabled(std::move(directMlEnabled)),
          m_shared(std::make_shared<SharedState>()), m_idleTimeoutMs(idleTimeoutMs),
          m_threads{}, m_workers{} {
        for (std::size_t index = 0; index < kWorkerCount; ++index) {
            m_threads[index] = std::make_unique<QThread>();
            m_threads[index]->setObjectName(
                QStringLiteral("ScreenshotOcrWorker-%1").arg(static_cast<int>(index)));
            m_workers[index] = new OcrWorker;
            m_workers[index]->moveToThread(m_threads[index].get());
            QObject::connect(m_threads[index].get(), &QThread::finished, m_workers[index],
                             &QObject::deleteLater);
            m_threads[index]->start();
        }
    }

    ~Impl() {
        shutdown();
    }

    RequestToken enqueue(RequestToken token, QImage image, const QRectF& canvasRect,
                         QObject* receiver, Completion completion) {
        if (m_shared->stopping.load(std::memory_order_acquire)) {
            return 0;
        }
        auto job = std::make_shared<Job>();
        job->token = token;
        job->image = std::move(image);
        job->canvasRect = canvasRect;
        job->receiver = receiver;
        job->completion = std::move(completion);
        job->directMlEnabled = m_directMlEnabled && m_directMlEnabled();
        QPointer<ScreenshotOcrRecognitionService> service(m_owner);
        job->receiverDestroyed = QObject::connect(
            receiver, &QObject::destroyed, m_owner, [service, token]() {
                if (service != nullptr) {
                    service->cancel(token);
                }
            });

        const auto queueIterator = m_queue.insert(m_queue.end(), job);
        m_requests.insert(token, RequestRecord{job, true, queueIterator});
        dispatch();
        return token;
    }

    void cancel(RequestToken token) {
        auto request = m_requests.find(token);
        if (request == m_requests.end()) {
            return;
        }
        request->job->cancelled.store(true, std::memory_order_release);
        QObject::disconnect(request->job->receiverDestroyed);
        if (request->queued) {
            m_queue.erase(request->queueIterator);
            m_requests.erase(request);
        }
    }

  private:
    using RequestToken = ScreenshotOcrRecognitionPort::RequestToken;
    using Completion = ScreenshotOcrRecognitionPort::Completion;

    struct SharedState {
        std::atomic_bool stopping{false};
    };

    struct Job {
        RequestToken token = 0;
        QImage image;
        QRectF canvasRect;
        QPointer<QObject> receiver;
        Completion completion;
        QMetaObject::Connection receiverDestroyed;
        std::atomic_bool cancelled{false};
        bool directMlEnabled = false;
    };

    using JobPtr = std::shared_ptr<Job>;
    // This FIFO is intentionally unbounded. Avoiding request rejection is the product policy;
    // callers must account for RAM growth if producers continuously outrun OCR workers.
    using Queue = std::list<JobPtr>;

    struct RequestRecord {
        JobPtr job;
        bool queued = false;
        Queue::iterator queueIterator;
        int workerIndex = -1;
    };

    void dispatch() {
        if (m_shared->stopping.load(std::memory_order_acquire)) {
            return;
        }
        for (std::size_t workerIndex = 0; workerIndex < kWorkerCount && !m_queue.empty();
             ++workerIndex) {
            if (m_activeJobs[workerIndex] != nullptr) {
                continue;
            }
            JobPtr job = m_queue.front();
            m_queue.pop_front();
            auto record = m_requests.find(job->token);
            if (record == m_requests.end()) {
                continue;
            }
            record->queued = false;
            record->workerIndex = static_cast<int>(workerIndex);
            m_activeJobs[workerIndex] = job;
            runJob(std::move(job), workerIndex);
        }
    }

    void runJob(JobPtr job, const std::size_t workerIndex) {
        QPointer<ScreenshotOcrRecognitionService> service(m_owner);
        const std::shared_ptr<SharedState> shared = m_shared;
        OcrWorker* worker = m_workers[workerIndex];
        QMetaObject::invokeMethod(
            worker,
            [worker, service, shared, job = std::move(job)]() mutable {
                ScreenshotOcrRecognitionResult result;
                if (!job->cancelled.load(std::memory_order_acquire)) {
                    result = worker->recognize(std::move(job->image), job->canvasRect,
                                               job->directMlEnabled);
                }
                if (shared->stopping.load(std::memory_order_acquire) || service == nullptr) {
                    return;
                }
                QMetaObject::invokeMethod(
                    service,
                    [service, job = std::move(job), result = std::move(result)]() mutable {
                        if (service != nullptr && service->m_impl != nullptr) {
                            service->m_impl->finishJob(std::move(job), std::move(result));
                        }
                    },
                    Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    }

    void finishJob(JobPtr job, ScreenshotOcrRecognitionResult result) {
        auto request = m_requests.find(job->token);
        if (request == m_requests.end()) {
            return;
        }
        const int workerIndex = request->workerIndex;
        if (workerIndex >= 0 && workerIndex < static_cast<int>(kWorkerCount)) {
            m_activeJobs[static_cast<std::size_t>(workerIndex)].reset();
        }
        QObject::disconnect(job->receiverDestroyed);
        Completion completion = std::move(job->completion);
        const QPointer<QObject> receiver = job->receiver;
        const bool cancelled = job->cancelled.load(std::memory_order_acquire);
        m_requests.erase(request);

        dispatch();
        if (m_queue.empty() && workerIndex >= 0 &&
            workerIndex < static_cast<int>(kWorkerCount)) {
            OcrWorker* worker = m_workers[static_cast<std::size_t>(workerIndex)];
            QMetaObject::invokeMethod(
                worker, [worker, timeout = m_idleTimeoutMs]() { worker->startIdleTimer(timeout); },
                Qt::QueuedConnection);
        }
        if (!cancelled && receiver != nullptr && completion) {
            completion(std::move(result));
        }
    }

    void shutdown() {
        if (m_shared->stopping.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        for (auto request = m_requests.begin(); request != m_requests.end(); ++request) {
            request->job->cancelled.store(true, std::memory_order_release);
            QObject::disconnect(request->job->receiverDestroyed);
        }
        m_queue.clear();
        m_requests.clear();
        for (std::size_t index = 0; index < kWorkerCount; ++index) {
            OcrWorker* worker = m_workers[index];
            QThread* thread = m_threads[index].get();
            QMetaObject::invokeMethod(
                worker,
                [worker, thread]() {
                    worker->shutdown();
                    thread->quit();
                },
                Qt::QueuedConnection);
            thread->wait();
            m_workers[index] = nullptr;
        }
    }

    ScreenshotOcrRecognitionService* m_owner = nullptr;
    std::function<bool()> m_directMlEnabled;
    std::shared_ptr<SharedState> m_shared;
    Queue m_queue;
    QHash<RequestToken, RequestRecord> m_requests;
    static constexpr std::size_t kWorkerCount = 2;
    std::array<JobPtr, kWorkerCount> m_activeJobs{};
    int m_idleTimeoutMs = ScreenshotOcrRecognitionService::Options{}.engineIdleTimeoutMs;
    std::array<std::unique_ptr<QThread>, kWorkerCount> m_threads;
    std::array<OcrWorker*, kWorkerCount> m_workers{};
};

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(QObject* parent)
    : ScreenshotOcrRecognitionService(Options{workerIdleTimeoutMs()}, {}, parent) {}

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(
    std::function<bool()> directMlEnabled, QObject* parent)
    : ScreenshotOcrRecognitionService(Options{workerIdleTimeoutMs()},
                                      std::move(directMlEnabled), parent) {}

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(
    const Options& options, std::function<bool()> directMlEnabled, QObject* parent)
    : ScreenshotOcrRecognitionPort(parent),
      m_impl(std::make_unique<Impl>(this, (std::max)(1, options.engineIdleTimeoutMs),
                                    std::move(directMlEnabled))) {}

ScreenshotOcrRecognitionService::~ScreenshotOcrRecognitionService() = default;

ScreenshotOcrRecognitionPort::RequestToken
ScreenshotOcrRecognitionService::recognize(QImage image, const QRectF& canvasRect,
                                           QObject* receiver, Completion completion) {
    if (image.isNull() || !canvasRect.isValid() || canvasRect.isEmpty() || receiver == nullptr ||
        !completion || m_impl == nullptr) {
        return 0;
    }
    do {
        ++m_nextToken;
    } while (m_nextToken == 0);
    return m_impl->enqueue(m_nextToken, std::move(image), canvasRect, receiver,
                           std::move(completion));
}

void ScreenshotOcrRecognitionService::cancel(RequestToken token) {
    if (m_impl != nullptr && token != 0) {
        m_impl->cancel(token);
    }
}
