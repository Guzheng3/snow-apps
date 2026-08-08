#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDSERVICE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDSERVICE_H

#include <QImage>

#include <QtGlobal>

#include <utility>

class ScreenshotClipboardPixelSource final {
  public:
    enum class Format {
        Unsupported,
        Argb32,
        Argb32Premultiplied,
        Rgba8888,
    };

    ScreenshotClipboardPixelSource() = default;
    explicit ScreenshotClipboardPixelSource(QImage image) : m_image(std::move(image)) {}

    [[nodiscard]] bool isValid() const {
        return !m_image.isNull() && m_image.width() > 0 && m_image.height() > 0 &&
               m_image.constBits() != nullptr;
    }
    [[nodiscard]] Format format() const {
        switch (m_image.format()) {
        case QImage::Format_ARGB32:
            return Format::Argb32;
        case QImage::Format_ARGB32_Premultiplied:
            return Format::Argb32Premultiplied;
        case QImage::Format_RGBA8888:
            return Format::Rgba8888;
        default:
            return Format::Unsupported;
        }
    }
    [[nodiscard]] const QImage& image() const { return m_image; }

  private:
    QImage m_image;
};

class QClipboard;
struct ScreenshotClipboardPayloadTestAccess;

class ScreenshotClipboardPayload final {
  public:
    ScreenshotClipboardPayload() = default;
    ~ScreenshotClipboardPayload();

    ScreenshotClipboardPayload(const ScreenshotClipboardPayload&) = delete;
    ScreenshotClipboardPayload& operator=(const ScreenshotClipboardPayload&) = delete;
    ScreenshotClipboardPayload(ScreenshotClipboardPayload&& other) noexcept;
    ScreenshotClipboardPayload& operator=(ScreenshotClipboardPayload&& other) noexcept;

    [[nodiscard]] bool isValid() const;

  private:
    friend class ScreenshotClipboardService;
    friend struct ScreenshotClipboardPayloadTestAccess;

    void reset() noexcept;

#if defined(Q_OS_WIN) || defined(_WIN32)
    void* m_nativeHandle = nullptr;
#else
    QImage m_image;
#endif
};

class ScreenshotClipboardService final {
  public:
    [[nodiscard]] static ScreenshotClipboardPayload prepare(
        ScreenshotClipboardPixelSource source);
    [[nodiscard]] static ScreenshotClipboardPayload prepareImage(const QImage& image);
    [[nodiscard]] static bool publish(QClipboard* clipboard, ScreenshotClipboardPayload payload);
    [[nodiscard]] static bool publishImage(QClipboard* clipboard, const QImage& image);
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDSERVICE_H
