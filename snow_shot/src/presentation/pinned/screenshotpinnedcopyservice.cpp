#include "snow_shot/presentation/screenshotpinnedcopyservice.h"

#include "snow_shot/presentation/screenshotdefaultstyles.h"

#include "snow_draw_engine_qt/snow_canvas_runtime.h"

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThread>

#include <utility>

namespace {
class ScreenshotPinnedCopyWorker final : public QObject {
  public:
    ScreenshotClipboardPayload prepareCurrentViewport(
        const ScreenshotPinnedViewportCopyRequest& request) {
        if (request.backgroundImage.isNull() || !request.backgroundCanvasRect.isValid() ||
            request.backgroundCanvasRect.isEmpty() || !request.contentPixelSize.isValid() ||
            request.contentPixelSize.isEmpty() || !ensureRuntime()) {
            return {};
        }
        if (!request.documentSession.isEmpty() &&
            !m_runtime->restoreDocumentSession(request.documentSession)) {
            return {};
        }

        const QList<CanvasExportSource> sources{
            CanvasExportSource{request.backgroundImage, request.backgroundCanvasRect}};
        QImage content = m_runtime->renderToImage(request.backgroundCanvasRect,
                                                  request.contentPixelSize, sources);
        if (content.isNull()) {
            return {};
        }
        return ScreenshotClipboardService::prepareImage(
            ScreenshotResultCompositor::compose(content, request.resultStyle));
    }

    ScreenshotClipboardPayload prepareOriginalImage(const QImage& image) const {
        return ScreenshotClipboardService::prepareImage(image);
    }

  private:
    bool ensureRuntime() {
        if (m_runtime == nullptr) {
            m_runtime = std::make_unique<SnowCanvasRuntime>(SnowCanvasRuntimeConfig{
                snow_shot::presentation::screenshotCanvasStyleDefaults()});
        }
        return m_runtime->isValid();
    }

    std::unique_ptr<SnowCanvasRuntime> m_runtime;
};
} // namespace

ScreenshotPinnedCopyService::ScreenshotPinnedCopyService()
    : m_thread(std::make_unique<QThread>()), m_worker(new ScreenshotPinnedCopyWorker),
      m_completionContext(new QObject) {
    m_thread->setObjectName(QStringLiteral("ScreenshotPinnedCopyWorker"));
    m_worker->moveToThread(m_thread.get());
    QObject::connect(m_thread.get(), &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

ScreenshotPinnedCopyService::~ScreenshotPinnedCopyService() {
    delete m_completionContext;
    m_completionContext = nullptr;
    if (m_thread != nullptr) {
        m_thread->quit();
        m_thread->wait();
    }
    m_worker = nullptr;
}

bool ScreenshotPinnedCopyService::beginRequest(RequestKind kind, quint64* generation) {
    if (generation == nullptr || kind == RequestKind::None || m_worker == nullptr ||
        m_thread == nullptr || !m_thread->isRunning() || m_completionContext == nullptr) {
        return false;
    }
    if (m_requestInFlight && m_activeKind == kind) {
        return false;
    }
    *generation = ++m_generation;
    m_activeKind = kind;
    m_requestInFlight = true;
    return true;
}

bool ScreenshotPinnedCopyService::requestCurrentViewport(
    ScreenshotPinnedViewportCopyRequest request, QObject* receiver, Callback callback) {
    if (receiver == nullptr || !callback || request.backgroundImage.isNull()) {
        return false;
    }
    quint64 generation = 0;
    if (!beginRequest(RequestKind::CurrentViewport, &generation)) {
        return false;
    }

    auto* worker = static_cast<ScreenshotPinnedCopyWorker*>(m_worker);
    const QPointer<QObject> guardedReceiver(receiver);
    const QPointer<QObject> guardedCompletionContext(m_completionContext);
    const bool scheduled = QMetaObject::invokeMethod(
        worker,
        [this, worker, guardedReceiver, guardedCompletionContext, generation,
         request = std::move(request), callback = std::move(callback)]() mutable {
            auto payload = std::make_shared<ScreenshotClipboardPayload>(
                worker->prepareCurrentViewport(request));
            if (guardedCompletionContext.isNull()) {
                return;
            }
            static_cast<void>(QMetaObject::invokeMethod(
                guardedCompletionContext,
                [this, guardedReceiver, guardedCompletionContext, generation, payload,
                 callback = std::move(callback)]() mutable {
                    if (guardedCompletionContext.isNull() || generation != m_generation) {
                        return;
                    }
                    m_requestInFlight = false;
                    m_activeKind = RequestKind::None;
                    if (!guardedReceiver.isNull()) {
                        callback(std::move(*payload));
                    }
                },
                Qt::QueuedConnection));
        },
        Qt::QueuedConnection);
    if (!scheduled) {
        m_requestInFlight = false;
        m_activeKind = RequestKind::None;
    }
    return scheduled;
}

bool ScreenshotPinnedCopyService::requestOriginalImage(QImage image, QObject* receiver,
                                                       Callback callback) {
    if (receiver == nullptr || !callback || image.isNull()) {
        return false;
    }
    quint64 generation = 0;
    if (!beginRequest(RequestKind::OriginalImage, &generation)) {
        return false;
    }

    auto* worker = static_cast<ScreenshotPinnedCopyWorker*>(m_worker);
    const QPointer<QObject> guardedReceiver(receiver);
    const QPointer<QObject> guardedCompletionContext(m_completionContext);
    const bool scheduled = QMetaObject::invokeMethod(
        worker,
        [this, worker, guardedReceiver, guardedCompletionContext, generation,
         image = std::move(image), callback = std::move(callback)]() mutable {
            auto payload = std::make_shared<ScreenshotClipboardPayload>(
                worker->prepareOriginalImage(image));
            if (guardedCompletionContext.isNull()) {
                return;
            }
            static_cast<void>(QMetaObject::invokeMethod(
                guardedCompletionContext,
                [this, guardedReceiver, guardedCompletionContext, generation, payload,
                 callback = std::move(callback)]() mutable {
                    if (guardedCompletionContext.isNull() || generation != m_generation) {
                        return;
                    }
                    m_requestInFlight = false;
                    m_activeKind = RequestKind::None;
                    if (!guardedReceiver.isNull()) {
                        callback(std::move(*payload));
                    }
                },
                Qt::QueuedConnection));
        },
        Qt::QueuedConnection);
    if (!scheduled) {
        m_requestInFlight = false;
        m_activeKind = RequestKind::None;
    }
    return scheduled;
}

void ScreenshotPinnedCopyService::invalidate() {
    ++m_generation;
    m_requestInFlight = false;
    m_activeKind = RequestKind::None;
}
