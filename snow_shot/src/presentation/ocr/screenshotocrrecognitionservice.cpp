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
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr int kWorkerIdleTimeoutMs = 30'000;

int workerIdleTimeoutMs() {
    bool valid = false;
    const int configured = qEnvironmentVariableIntValue("SNOW_SHOT_OCR_IDLE_TIMEOUT_MS", &valid);
    return valid && configured > 0 ? configured : kWorkerIdleTimeoutMs;
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
    ScreenshotOcrRecognitionResult recognize(QImage source, const QRectF& canvasRect,
                                             bool directMlEnabled) {
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

  private:
    bool ensureEngine(bool directMlEnabled) {
        if (m_engine != nullptr && m_engineDirectMl == directMlEnabled) {
            return true;
        }
        m_engine.reset();
        const SnowOcrEngineConfigV1 config{
            static_cast<std::uint32_t>(sizeof(SnowOcrEngineConfigV1)),
            1,
            1,
            1,
            0,
            static_cast<std::uint8_t>(directMlEnabled ? 1 : 0),
            {0, 0},
        };
        m_engine.reset(snow_ocr_engine_create_with_config_v1(&config));
        m_engineDirectMl = directMlEnabled;
        return m_engine != nullptr;
    }

    OcrEngineHandle m_engine;
    bool m_engineDirectMl = false;
};
} // namespace

class ScreenshotOcrRecognitionService::Impl final {
  public:
    Impl(ScreenshotOcrRecognitionService* owner, std::function<bool()> directMlEnabled)
        : m_owner(owner), m_directMlEnabled(std::move(directMlEnabled)),
          m_shared(std::make_shared<SharedState>()), m_maxWorkers(workerBudget()),
          m_idleTimeoutMs(workerIdleTimeoutMs()) {}

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
        reconcileCapacity();
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
            reconcileCapacity();
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
    };

    struct WorkerSlot {
        quint64 id = 0;
        QThread* thread = nullptr;
        OcrWorker* worker = nullptr;
        QTimer* idleTimer = nullptr;
        JobPtr activeJob;
        bool retiring = false;
    };

    static int workerBudget() {
        SnowOcrRuntimeInfoV1 info{};
        info.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrRuntimeInfoV1));
        int physicalCores = 0;
        if (snow_ocr_runtime_info_v1(&info) != 0) {
            physicalCores = static_cast<int>((std::min)(
                info.physical_core_count,
                static_cast<std::uint32_t>((std::numeric_limits<int>::max)())));
        }
        if (physicalCores < 1) {
            physicalCores = (std::max)(1, QThread::idealThreadCount());
        }
        return (std::max)(1, physicalCores / 2);
    }

    WorkerSlot* findSlot(quint64 id) const {
        const auto slot = std::find_if(m_workers.begin(), m_workers.end(), [id](const auto& item) {
            return item->id == id;
        });
        return slot != m_workers.end() ? slot->get() : nullptr;
    }

    int busyWorkerCount() const {
        return static_cast<int>(std::count_if(m_workers.begin(), m_workers.end(), [](const auto& slot) {
            return !slot->retiring && slot->activeJob != nullptr;
        }));
    }

    int liveWorkerCount() const {
        return static_cast<int>(std::count_if(m_workers.begin(), m_workers.end(), [](const auto& slot) {
            return !slot->retiring;
        }));
    }

    void reconcileCapacity() {
        if (m_shared->stopping.load(std::memory_order_acquire)) {
            return;
        }
        const std::size_t outstanding = m_queue.size() + static_cast<std::size_t>(busyWorkerCount());
        const int desired = static_cast<int>((std::min)(
            static_cast<std::size_t>(m_maxWorkers), outstanding));
        while (liveWorkerCount() < desired) {
            startWorker();
        }
    }

    void startWorker() {
        auto slot = std::make_unique<WorkerSlot>();
        slot->id = ++m_nextWorkerId;
        slot->thread = new QThread;
        slot->worker = new OcrWorker;
        slot->idleTimer = new QTimer(m_owner);
        slot->idleTimer->setSingleShot(true);
        slot->worker->moveToThread(slot->thread);
        slot->thread->setObjectName(QStringLiteral("ScreenshotOcrWorker-%1").arg(slot->id));
        const quint64 id = slot->id;
        QObject::connect(slot->idleTimer, &QTimer::timeout, m_owner,
                         [this, id]() { retireIdleWorker(id); });
        QObject::connect(slot->thread, &QThread::finished, m_owner,
                         [this, id]() { handleWorkerFinished(id); });
        slot->thread->start();
        m_workers.push_back(std::move(slot));
    }

    void dispatch() {
        if (m_shared->stopping.load(std::memory_order_acquire)) {
            return;
        }
        for (const auto& slot : m_workers) {
            if (m_queue.empty()) {
                break;
            }
            if (slot->retiring || slot->activeJob != nullptr) {
                continue;
            }
            slot->idleTimer->stop();
            JobPtr job = m_queue.front();
            m_queue.pop_front();
            auto record = m_requests.find(job->token);
            if (record == m_requests.end()) {
                continue;
            }
            record->queued = false;
            slot->activeJob = job;
            runJob(*slot, std::move(job));
        }
        if (m_queue.empty()) {
            for (const auto& slot : m_workers) {
                if (!slot->retiring && slot->activeJob == nullptr && !slot->idleTimer->isActive()) {
                    slot->idleTimer->start(m_idleTimeoutMs);
                }
            }
        }
    }

    void runJob(WorkerSlot& slot, JobPtr job) {
        QPointer<ScreenshotOcrRecognitionService> service(m_owner);
        const std::shared_ptr<SharedState> shared = m_shared;
        OcrWorker* worker = slot.worker;
        const quint64 workerId = slot.id;
        QMetaObject::invokeMethod(
            worker,
            [worker, service, shared, workerId, job = std::move(job)]() mutable {
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
                    [service, workerId, job = std::move(job), result = std::move(result)]() mutable {
                        if (service != nullptr && service->m_impl != nullptr) {
                            service->m_impl->finishJob(workerId, std::move(job), std::move(result));
                        }
                    },
                    Qt::QueuedConnection);
            },
            Qt::QueuedConnection);
    }

    void finishJob(quint64 workerId, JobPtr job, ScreenshotOcrRecognitionResult result) {
        WorkerSlot* slot = findSlot(workerId);
        if (slot == nullptr || slot->activeJob == nullptr ||
            slot->activeJob->token != job->token) {
            return;
        }
        slot->activeJob.reset();
        auto request = m_requests.find(job->token);
        if (request == m_requests.end()) {
            reconcileCapacity();
            dispatch();
            return;
        }
        QObject::disconnect(job->receiverDestroyed);
        Completion completion = std::move(job->completion);
        const QPointer<QObject> receiver = job->receiver;
        const bool cancelled = job->cancelled.load(std::memory_order_acquire);
        m_requests.erase(request);

        reconcileCapacity();
        dispatch();
        if (!cancelled && receiver != nullptr && completion) {
            completion(std::move(result));
        }
    }

    void retireIdleWorker(quint64 id) {
        WorkerSlot* slot = findSlot(id);
        if (slot == nullptr || slot->retiring || slot->activeJob != nullptr || !m_queue.empty()) {
            return;
        }
        slot->retiring = true;
        slot->idleTimer->stop();
        OcrWorker* worker = slot->worker;
        QThread* thread = slot->thread;
        QMetaObject::invokeMethod(
            worker,
            [worker, thread]() {
                delete worker;
                thread->quit();
            },
            Qt::QueuedConnection);
    }

    void handleWorkerFinished(quint64 id) {
        if (m_shared->stopping.load(std::memory_order_acquire)) {
            return;
        }
        const auto slot = std::find_if(m_workers.begin(), m_workers.end(), [id](const auto& item) {
            return item->id == id;
        });
        if (slot == m_workers.end()) {
            return;
        }
        (*slot)->thread->wait();
        delete (*slot)->thread;
        delete (*slot)->idleTimer;
        m_workers.erase(slot);
        reconcileCapacity();
        dispatch();
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

        for (const auto& slot : m_workers) {
            slot->idleTimer->stop();
            if (slot->retiring) {
                continue;
            }
            slot->retiring = true;
            OcrWorker* worker = slot->worker;
            QThread* thread = slot->thread;
            QMetaObject::invokeMethod(
                worker,
                [worker, thread]() {
                    delete worker;
                    thread->quit();
                },
                Qt::QueuedConnection);
        }
        for (const auto& slot : m_workers) {
            slot->thread->wait();
            delete slot->thread;
            delete slot->idleTimer;
            slot->activeJob.reset();
        }
        m_workers.clear();
    }

    ScreenshotOcrRecognitionService* m_owner = nullptr;
    std::function<bool()> m_directMlEnabled;
    std::shared_ptr<SharedState> m_shared;
    Queue m_queue;
    QHash<RequestToken, RequestRecord> m_requests;
    std::vector<std::unique_ptr<WorkerSlot>> m_workers;
    quint64 m_nextWorkerId = 0;
    int m_maxWorkers = 1;
    int m_idleTimeoutMs = kWorkerIdleTimeoutMs;
};

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(QObject* parent)
    : ScreenshotOcrRecognitionService({}, parent) {}

ScreenshotOcrRecognitionService::ScreenshotOcrRecognitionService(
    std::function<bool()> directMlEnabled, QObject* parent)
    : ScreenshotOcrRecognitionPort(parent),
      m_impl(std::make_unique<Impl>(this, std::move(directMlEnabled))) {}

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
