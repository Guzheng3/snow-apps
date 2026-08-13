#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONEXPORTUISERVICES_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONEXPORTUISERVICES_H

#include "snow_shot/presentation/screenshotselectionexportworkflowports.h"

#include <functional>
#include <memory>

class SnowCanvasRuntime;
class QScreen;
class ScreenshotOcrRecognitionPort;
class ScreenshotQrRecognitionPort;
class SnowShotApiClient;
class ScreenshotPinnedWindowPool;

class ScreenshotSelectionExportUiServices final : public ScreenshotSelectionExportDestinationPort {
  public:
    explicit ScreenshotSelectionExportUiServices(
        SnowCanvasRuntime& runtime, ScreenshotOcrRecognitionPort* recognition = nullptr,
        ScreenshotQrRecognitionPort* qrRecognition = nullptr,
        SnowShotApiClient* tableRecognition = nullptr,
        std::function<void()> showMainWindowRequested = {});
    ~ScreenshotSelectionExportUiServices() override;

    [[nodiscard]] bool publishClipboard(ScreenshotClipboardPayload payload) override;
    void setClipboardImage(const QImage& image);
    [[nodiscard]] bool presentPinnedImage(const QImage& image, QScreen* screen,
                                          const QRect& nativeGeometry,
                                          const QSize& fullResolutionScaleBasis = {});
    [[nodiscard]] bool
    presentPinnedSelection(const ScreenshotPinnedSelectionRequest& request) override;

  private:
    SnowCanvasRuntime& m_runtime;
    ScreenshotOcrRecognitionPort* m_recognition = nullptr;
    ScreenshotQrRecognitionPort* m_qrRecognition = nullptr;
    SnowShotApiClient* m_tableRecognition = nullptr;
    std::function<void()> m_showMainWindowRequested;
    std::unique_ptr<ScreenshotPinnedWindowPool> m_windowPool;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTSELECTIONEXPORTUISERVICES_H
