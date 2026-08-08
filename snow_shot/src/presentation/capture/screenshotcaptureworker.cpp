#include "screenshotcaptureworker.h"

#include "snow_shot/presentation/screenshotcapturecoordinator.h"

#include "snow_capture.h"

#include <QDebug>
#include <QImage>
#include <QMetaObject>

#include <limits>
#include <utility>

namespace {
void releaseFrameLease(void* lease) {
    snow_capture_frame_lease_release(static_cast<SnowCaptureFrameLease*>(lease));
}

bool validFrameInfo(const SnowCaptureFrameInfo& info) {
    if (info.rgba_bytes == nullptr || info.rgba_len == 0 || info.width == 0 || info.height == 0) {
        return false;
    }

    const quint64 expectedStride = static_cast<quint64>(info.width) * 4ULL;
    const quint64 expectedLen = expectedStride * static_cast<quint64>(info.height);
    return expectedStride <= std::numeric_limits<std::uint32_t>::max() &&
           info.stride_bytes == static_cast<std::uint32_t>(expectedStride) &&
           info.rgba_len >= expectedLen;
}

QImage imageFromSnapshotFrame(SnowCaptureSnapshot* snapshot, size_t index,
                              const SnowCaptureFrameInfo& info) {
    if (snapshot == nullptr || !validFrameInfo(info)) {
        return {};
    }

    SnowCaptureFrameLease* lease = snow_capture_snapshot_frame_retain(snapshot, index);
    if (lease == nullptr) {
        return {};
    }

    QImage image(info.rgba_bytes, static_cast<int>(info.width), static_cast<int>(info.height),
                 static_cast<int>(info.stride_bytes), QImage::Format_RGBA8888, &releaseFrameLease,
                 lease);
    if (image.isNull()) {
        snow_capture_frame_lease_release(lease);
    }
    return image;
}
} // namespace

ScreenshotCaptureWorker::~ScreenshotCaptureWorker() {
    if (m_session != nullptr) {
        snow_capture_desktop_session_destroy(m_session);
        m_session = nullptr;
    }
}

void ScreenshotCaptureWorker::prepare(quint64 requestId,
                                      const QPointer<ScreenshotCaptureCoordinator>& coordinator) {
    const bool ok = prepareSessionIfNeeded();
    postPrepared(requestId, coordinator, ok);
}

void ScreenshotCaptureWorker::refreshLayout(quint64 requestId) {
    Q_UNUSED(requestId);
    if (ensureSession()) {
        snow_capture_desktop_session_refresh_layout(m_session);
    }
}

void ScreenshotCaptureWorker::releaseIdleResources(quint64 requestId) {
    Q_UNUSED(requestId);
    if (m_session != nullptr &&
        snow_capture_desktop_session_release_idle_resources(m_session) == 0) {
        qWarning("Failed to release desktop capture idle resources: %s",
                 snow_capture_last_error_message());
    }
}

void ScreenshotCaptureWorker::captureAll(quint64 requestId,
                                         const QPointer<ScreenshotCaptureCoordinator>& coordinator,
                                         bool refreshLayout) {
    QVector<CapturedDisplayModel> displays;
    if (!ensureSession()) {
        postCaptureResult(requestId, coordinator, std::move(displays));
        return;
    }

    if (refreshLayout) {
        if (snow_capture_desktop_session_refresh_layout(m_session) == 0) {
            qWarning("Failed to refresh desktop capture layout: %s",
                     snow_capture_last_error_message());
            postCaptureResult(requestId, coordinator, std::move(displays));
            return;
        }
    }

    SnowCaptureSnapshot* captureSnapshot = nullptr;
    captureSnapshot = snow_capture_desktop_session_capture_all(m_session);
    if (captureSnapshot == nullptr) {
        postCaptureResult(requestId, coordinator, std::move(displays));
        return;
    }

    const size_t count = snow_capture_snapshot_count(captureSnapshot);
    displays.reserve(static_cast<int>(count));
    for (size_t index = 0; index < count; ++index) {
        SnowCaptureFrameInfo info{};
        if (snow_capture_snapshot_frame_info(captureSnapshot, index, &info) == 0) {
            continue;
        }

        QImage image;
        image = imageFromSnapshotFrame(captureSnapshot, index, info);
        if (image.isNull()) {
            continue;
        }

        CapturedDisplayModel display;
        display.stableId = QString::fromUtf8(info.stable_id != nullptr ? info.stable_id : "");
        display.name = QString::fromUtf8(info.name != nullptr ? info.name : "");
        display.physicalRect =
            QRect(info.x, info.y, static_cast<int>(info.width), static_cast<int>(info.height));
        display.canvasRect = display.physicalRect;
        display.image = std::move(image);
        display.active = true;
        displays.push_back(std::move(display));
    }

    snow_capture_snapshot_destroy(captureSnapshot);

    postCaptureResult(requestId, coordinator, std::move(displays));
}

bool ScreenshotCaptureWorker::ensureSession() {
    if (m_session != nullptr) {
        return true;
    }

    SnowCaptureDesktopSessionConfig config{};
    config.capture_retry_count = 1;
    m_session = snow_capture_desktop_session_create(&config);
    if (m_session == nullptr) {
        qWarning("Failed to create desktop capture session: %s", snow_capture_last_error_message());
        return false;
    }
    return true;
}

bool ScreenshotCaptureWorker::sessionPrepared() const {
    if (m_session == nullptr) {
        return false;
    }

    SnowCaptureDesktopSessionState state{};
    if (snow_capture_desktop_session_state(m_session, &state) == 0) {
        return false;
    }
    return state.prepared != 0;
}

bool ScreenshotCaptureWorker::prepareSessionIfNeeded() {
    if (!ensureSession()) {
        return false;
    }
    if (sessionPrepared()) {
        return true;
    }
    return snow_capture_desktop_session_prepare(m_session) != 0;
}

void ScreenshotCaptureWorker::postPrepared(
    quint64 requestId, const QPointer<ScreenshotCaptureCoordinator>& coordinator, bool ok) {
    if (coordinator.isNull()) {
        return;
    }

    QMetaObject::invokeMethod(
        coordinator,
        [coordinator, requestId, ok]() {
            if (!coordinator.isNull()) {
                emit coordinator->prepared(requestId, ok);
            }
        },
        Qt::QueuedConnection);
}

void ScreenshotCaptureWorker::postCaptureResult(
    quint64 requestId, const QPointer<ScreenshotCaptureCoordinator>& coordinator,
    QVector<CapturedDisplayModel> displays) {
    if (coordinator.isNull()) {
        return;
    }

    QMetaObject::invokeMethod(
        coordinator,
        [coordinator, requestId, displays = std::move(displays)]() mutable {
            if (!coordinator.isNull()) {
                emit coordinator->captureFinished(requestId, std::move(displays));
            }
        },
        Qt::QueuedConnection);
}
