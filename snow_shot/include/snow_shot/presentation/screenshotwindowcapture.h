#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTWINDOWCAPTURE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTWINDOWCAPTURE_H

#include <QImage>
#include <QRect>
#include <QString>
#include <QtGlobal>

#include <memory>
#include <optional>

struct ScreenshotWindowCaptureFrame {
    QImage image;
    QRect physicalRect;

    [[nodiscard]] bool isValid() const {
        return !image.isNull() && !physicalRect.isEmpty() && physicalRect.size() == image.size();
    }
};

// Owns one native window capture session. Captured pixels are copied before
// returning so callers do not depend on the lifetime of the Rust frame.
class ScreenshotWindowCapture final {
  public:
    explicit ScreenshotWindowCapture(quintptr nativeWindowHandle);
    ~ScreenshotWindowCapture();

    ScreenshotWindowCapture(const ScreenshotWindowCapture&) = delete;
    ScreenshotWindowCapture& operator=(const ScreenshotWindowCapture&) = delete;
    ScreenshotWindowCapture(ScreenshotWindowCapture&&) noexcept;
    ScreenshotWindowCapture& operator=(ScreenshotWindowCapture&&) noexcept;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool prepare();
    [[nodiscard]] std::optional<ScreenshotWindowCaptureFrame> capture();
    [[nodiscard]] QString errorMessage() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTWINDOWCAPTURE_H
