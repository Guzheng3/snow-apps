#include "snow_shot/presentation/screenshotcapturecoordinator.h"

#include "screenshotcaptureworker.h"

#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <utility>

ScreenshotCaptureCoordinator::ScreenshotCaptureCoordinator(QObject* parent) : QObject(parent) {}

ScreenshotCaptureCoordinator::~ScreenshotCaptureCoordinator() {
    shutdown();
}

bool ScreenshotCaptureCoordinator::hasWorker() const {
    return m_worker != nullptr;
}

void ScreenshotCaptureCoordinator::prepareAsync(quint64 requestId) {
    const QPointer<ScreenshotCaptureCoordinator> coordinator(this);
    static_cast<void>(postWorkerTask([coordinator, requestId](ScreenshotCaptureWorker& worker) {
        worker.prepare(requestId, coordinator);
    }));
}

void ScreenshotCaptureCoordinator::refreshLayoutAsync(quint64 requestId) {
    static_cast<void>(postWorkerTask(
        [requestId](ScreenshotCaptureWorker& worker) { worker.refreshLayout(requestId); }));
}

void ScreenshotCaptureCoordinator::captureAllAsync(quint64 requestId, bool refreshLayout) {
    const QPointer<ScreenshotCaptureCoordinator> coordinator(this);
    static_cast<void>(
        postWorkerTask([coordinator, requestId, refreshLayout](ScreenshotCaptureWorker& worker) {
            worker.captureAll(requestId, coordinator, refreshLayout);
        }));
}

void ScreenshotCaptureCoordinator::releaseIdleResourcesAsync(quint64 requestId) {
    if (!hasWorker()) {
        return;
    }

    static_cast<void>(postWorkerTask(
        [requestId](ScreenshotCaptureWorker& worker) { worker.releaseIdleResources(requestId); }));
}

void ScreenshotCaptureCoordinator::shutdown() {
    if (m_thread == nullptr) {
        m_worker = nullptr;
        return;
    }

    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_worker = nullptr;
}

void ScreenshotCaptureCoordinator::ensureWorker() {
    if (hasWorker()) {
        return;
    }

    shutdown();
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("snow-shot-capture"));
    m_worker = new ScreenshotCaptureWorker;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

template <typename Task> bool ScreenshotCaptureCoordinator::postWorkerTask(Task&& task) {
    ensureWorker();
    if (!hasWorker()) {
        return false;
    }

    const QPointer<ScreenshotCaptureWorker> worker(m_worker);
    return QMetaObject::invokeMethod(
        m_worker,
        [worker, task = std::forward<Task>(task)]() mutable {
            if (!worker.isNull()) {
                task(*worker);
            }
        },
        Qt::QueuedConnection);
}
