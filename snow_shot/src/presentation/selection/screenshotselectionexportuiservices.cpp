#include "snow_shot/presentation/screenshotselectionexportuiservices.h"

#include "snow_shot/presentation/screenshotpinnedwindow.h"
#include "snow_shot/presentation/screenshotocrrecognitionservice.h"
#include "snow_shot/presentation/screenshotqrrecognitionservice.h"
#include "snow_shot/network/snowshotapiclient.h"
#include "snow_shot/presentation/screenshotclipboardservice.h"
#include "../pinned/screenshotpintoperfinstrumentation.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <QPointer>
#include <QScreen>
#include <QTimer>

#include <algorithm>

#if defined(Q_OS_WIN) || defined(_WIN32)
#include <Windows.h>
#include <dwmapi.h>
#endif

class ScreenshotPinnedWindowPool final : public QObject {
  public:
    explicit ScreenshotPinnedWindowPool(QObject* parent = nullptr) : QObject(parent) {
        scheduleReplenish();
    }

    ~ScreenshotPinnedWindowPool() override {
        if (m_spare != nullptr) {
            delete m_spare;
            m_spare = nullptr;
        }
    }

    ScreenshotPinnedWindow* acquire(ScreenshotPinnedWindow::RuntimeMode mode,
                                    SnowCanvasRuntime* sourceRuntime) {
        ScreenshotPinnedWindow* window = m_spare;
        bool usedSpare = window != nullptr;
        if (usedSpare) {
            m_spare = nullptr;
        }

        if (window != nullptr && sourceRuntime != nullptr &&
            !window->prepareDocument(*sourceRuntime)) {
            window->deleteLater();
            window = nullptr;
            usedSpare = false;
        }
        if (window == nullptr) {
            const QElapsedTimer timer = [&]() {
                QElapsedTimer value;
                value.start();
                return value;
            }();
            window = new ScreenshotPinnedWindow(mode);
            if (sourceRuntime != nullptr && !window->prepareDocument(*sourceRuntime)) {
                window->deleteLater();
                window = nullptr;
            }
            if (window != nullptr) {
                SNOW_SHOT_PIN_PERF_COUNTER("shell.construction_ns", timer.nsecsElapsed());
            }
        }

        SNOW_SHOT_PIN_PERF_COUNTER(usedSpare ? "shell.hit" : "shell.miss", 1);
        scheduleReplenish();
        return window;
    }

  private:
    void scheduleReplenish() {
        if (m_replenishQueued) {
            return;
        }
        m_replenishQueued = true;
        QTimer::singleShot(0, this, [this]() {
            m_replenishQueued = false;
            if (m_spare != nullptr) {
                return;
            }
            auto* spare = new ScreenshotPinnedWindow(ScreenshotPinnedWindow::RuntimeMode::NoDocument);
            if (!spare->prewarm(QGuiApplication::primaryScreen())) {
                spare->deleteLater();
                return;
            }
            m_spare = spare;
        });
    }

    QPointer<ScreenshotPinnedWindow> m_spare;
    bool m_replenishQueued = false;
};

namespace {
bool presentPinnedWindowAndSynchronize(ScreenshotPinnedWindow* window,
                                       const ScreenshotPinnedWindow::Config& config) {
    if (window == nullptr) {
        return false;
    }
    SNOW_SHOT_PIN_PERF_MILESTONE("ui.pinned_window_constructed");
    if (!window->present(config)) {
        window->deleteLater();
        return false;
    }
    SNOW_SHOT_PIN_PERF_COUNTER("window.visible", window->isVisible() ? 1 : 0);
    SNOW_SHOT_PIN_PERF_COUNTER("window.geometry_valid",
                               window->currentNativeGeometry() == config.nativeGeometry ? 1 : 0);
    SNOW_SHOT_PIN_PERF_MILESTONE("window.present_returned");
#if defined(SNOW_SHOT_PIN_PERF_INSTRUMENTATION) && (defined(Q_OS_WIN) || defined(_WIN32))
    DwmFlush();
#endif
    SNOW_SHOT_PIN_PERF_MILESTONE("window.dwm_flushed");
    return true;
}
} // namespace

ScreenshotSelectionExportUiServices::ScreenshotSelectionExportUiServices(
    SnowCanvasRuntime& runtime, ScreenshotOcrRecognitionPort* recognition,
    ScreenshotQrRecognitionPort* qrRecognition, SnowShotApiClient* tableRecognition)
    : m_runtime(runtime),
      m_recognition(recognition),
      m_qrRecognition(qrRecognition),
      m_tableRecognition(tableRecognition),
      m_windowPool(std::make_unique<ScreenshotPinnedWindowPool>()) {}

ScreenshotSelectionExportUiServices::~ScreenshotSelectionExportUiServices() = default;

bool ScreenshotSelectionExportUiServices::publishClipboard(ScreenshotClipboardPayload payload) {
    return ScreenshotClipboardService::publish(QApplication::clipboard(), std::move(payload));
}

void ScreenshotSelectionExportUiServices::setClipboardImage(const QImage& image) {
    static_cast<void>(ScreenshotClipboardService::publishImage(QApplication::clipboard(), image));
}

bool ScreenshotSelectionExportUiServices::presentPinnedSelection(
    const ScreenshotPinnedSelectionRequest& request) {
    SNOW_SHOT_PIN_PERF_SCOPE("ui.present_pinned_selection");
    if (!request.isValid()) {
        return false;
    }

    auto* pinnedWindow = m_windowPool != nullptr
                             ? m_windowPool->acquire(ScreenshotPinnedWindow::RuntimeMode::CloneDocument,
                                                     &m_runtime)
                             : nullptr;
    if (pinnedWindow == nullptr) {
        pinnedWindow = new ScreenshotPinnedWindow(m_runtime);
    }
    ScreenshotPinnedWindow::Config config;
    config.nativeGeometry = request.geometry.nativeGeometry;
    config.canvasSourceRect = request.contentCanvasRect;
    config.contentCanvasRect = request.contentCanvasRect;
    config.surfaceCanvasRect = request.surfaceCanvasRect;
    config.resultStyle = request.resultStyle;
    config.fullResolutionScaleBasis = request.fullResolutionScaleBasis;
    config.imageSource = request.imageSource;
    config.screen = request.screen;
    config.recognition = m_recognition;
    config.qrRecognition = m_qrRecognition;
    config.tableRecognition = m_tableRecognition;
    return presentPinnedWindowAndSynchronize(pinnedWindow, config);
}

bool ScreenshotSelectionExportUiServices::presentPinnedImage(const QImage& image, QScreen* screen,
                                                             const QRect& nativeGeometry,
                                                             const QSize& fullResolutionScaleBasis) {
    SNOW_SHOT_PIN_PERF_SCOPE("ui.present_pinned_image");
    if (image.isNull() || screen == nullptr || nativeGeometry.isEmpty()) {
        return false;
    }

    SNOW_SHOT_PIN_PERF_COUNTER("source.mode.materialized", 1);
    SNOW_SHOT_PIN_PERF_COUNTER("source.retained_bytes", image.sizeInBytes());

    auto* pinnedWindow = m_windowPool != nullptr
                             ? m_windowPool->acquire(ScreenshotPinnedWindow::RuntimeMode::NoDocument,
                                                     nullptr)
                             : nullptr;
    if (pinnedWindow == nullptr) {
        pinnedWindow =
            new ScreenshotPinnedWindow(ScreenshotPinnedWindow::RuntimeMode::NoDocument);
    }
    ScreenshotPinnedWindow::Config config;
    config.nativeGeometry = nativeGeometry;
    config.canvasSourceRect = QRectF(QPointF(0.0, 0.0), QSizeF(image.size()));
    config.imageSource = ScreenshotImageSource::fromImage(image, config.canvasSourceRect);
    config.contentCanvasRect = config.canvasSourceRect;
    config.surfaceCanvasRect = config.canvasSourceRect;
    config.fullResolutionScaleBasis = fullResolutionScaleBasis.isEmpty()
                                          ? image.size()
                                          : fullResolutionScaleBasis;
    config.initialScalePercent = 100.0 * nativeGeometry.width() /
                                 (std::max)(1, config.fullResolutionScaleBasis.width());
    config.screen = screen;
    config.enableEditing = true;
    config.recognition = m_recognition;
    config.qrRecognition = m_qrRecognition;
    config.tableRecognition = m_tableRecognition;
    return presentPinnedWindowAndSynchronize(pinnedWindow, config);
}
