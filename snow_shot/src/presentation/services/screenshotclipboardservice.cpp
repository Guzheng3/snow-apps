#include "snow_shot/presentation/screenshotclipboardservice.h"

#include "screenshotclipboardperfinstrumentation.h"

#include <QClipboard>
#include <QDebug>

#include <cstring>
#include <limits>

#if defined(Q_OS_WIN) || defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace {
HWND clipboardOwnerWindow() {
    static const HWND owner = CreateWindowExW(0, L"STATIC", L"SnowShotClipboardOwner", 0, 0, 0,
                                               0, 0, HWND_MESSAGE, nullptr,
                                               GetModuleHandleW(nullptr), nullptr);
    return owner;
}

HGLOBAL prepareDibV5(const ScreenshotClipboardPixelSource& source) {
    SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.prepare_total");
    if (!source.isValid()) {
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.invalid_image", 1);
        return nullptr;
    }

    const QImage& sourceImage = source.image();

    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.input_width", sourceImage.width());
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.input_height", sourceImage.height());
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.input_bytes", sourceImage.sizeInBytes());
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.input_format",
                                     static_cast<qint64>(sourceImage.format()));

    QImage image;
    if (source.format() == ScreenshotClipboardPixelSource::Format::Argb32 ||
        source.format() == ScreenshotClipboardPixelSource::Format::Rgba8888) {
        image = sourceImage;
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.fused_source", 1);
    } else {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.convert_argb32");
        image = sourceImage.convertToFormat(QImage::Format_ARGB32);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.compatibility_conversion", 1);
    }
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER(
        "clipboard.conversion_detached",
        image.constBits() != sourceImage.constBits() ? 1 : 0);

    const quint64 stride = static_cast<quint64>(image.width()) * 4;
    const quint64 pixelBytes = stride * static_cast<quint64>(image.height());
    const quint64 totalBytes = sizeof(BITMAPV5HEADER) + pixelBytes;
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.pixel_bytes",
                                     static_cast<qint64>(pixelBytes));
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.dib_bytes",
                                     static_cast<qint64>(totalBytes));
    if (pixelBytes > std::numeric_limits<DWORD>::max() ||
        totalBytes > std::numeric_limits<SIZE_T>::max()) {
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.image_too_large", 1);
        qWarning("Screenshot clipboard image is too large for CF_DIBV5");
        return nullptr;
    }

    HGLOBAL allocation = nullptr;
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.global_alloc");
        allocation = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(totalBytes));
    }
    if (allocation == nullptr) {
        const DWORD error = GetLastError();
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.global_alloc", 1);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.last_error", error);
        qWarning("Failed to allocate CF_DIBV5 clipboard image");
        return nullptr;
    }

    void* memory = nullptr;
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.global_lock");
        memory = GlobalLock(allocation);
    }
    if (memory == nullptr) {
        const DWORD error = GetLastError();
        GlobalFree(allocation);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.global_lock", 1);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.last_error", error);
        qWarning("Failed to lock CF_DIBV5 clipboard image");
        return nullptr;
    }

    auto* header = static_cast<BITMAPV5HEADER*>(memory);
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.initialize_header");
        std::memset(header, 0, sizeof(*header));
        header->bV5Size = sizeof(BITMAPV5HEADER);
        header->bV5Width = image.width();
        header->bV5Height = -image.height();
        header->bV5Planes = 1;
        header->bV5BitCount = 32;
        header->bV5Compression = BI_BITFIELDS;
        header->bV5SizeImage = static_cast<DWORD>(pixelBytes);
        header->bV5RedMask = 0x00ff0000;
        header->bV5GreenMask = 0x0000ff00;
        header->bV5BlueMask = 0x000000ff;
        header->bV5AlphaMask = 0xff000000;
        header->bV5CSType = LCS_sRGB;
        header->bV5Intent = LCS_GM_IMAGES;
    }

    auto* destination = reinterpret_cast<uchar*>(header + 1);
    if (source.format() == ScreenshotClipboardPixelSource::Format::Rgba8888) {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.fused_rgba8888");
        const auto* sourcePixels = image.constBits();
        for (int row = 0; row < image.height(); ++row) {
            const auto* sourceRow = sourcePixels + static_cast<quint64>(row) * image.bytesPerLine();
            auto* destinationRow = destination + static_cast<quint64>(row) * stride;
            for (int column = 0; column < image.width(); ++column) {
                const auto* pixel = sourceRow + static_cast<std::size_t>(column) * 4;
                auto* output = destinationRow + static_cast<std::size_t>(column) * 4;
                output[0] = pixel[2];
                output[1] = pixel[1];
                output[2] = pixel[0];
                output[3] = pixel[3];
            }
        }
    } else {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.copy_pixels");
        for (int row = 0; row < image.height(); ++row) {
            std::memcpy(destination + static_cast<quint64>(row) * stride,
                        image.constScanLine(row), static_cast<std::size_t>(stride));
        }
    }
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.global_unlock");
        GlobalUnlock(allocation);
    }
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.prepared", 1);
    return allocation;
}

bool publishDibV5(void** handle) {
    SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.publish_total");
    if (handle == nullptr || *handle == nullptr) {
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.invalid_payload", 1);
        return false;
    }

    const HGLOBAL allocation = static_cast<HGLOBAL>(*handle);
    HWND owner = nullptr;
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.owner_window");
        owner = clipboardOwnerWindow();
    }
    BOOL opened = FALSE;
    if (owner != nullptr) {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.open");
        opened = OpenClipboard(owner);
    }
    if (owner == nullptr || opened == FALSE) {
        const DWORD error = GetLastError();
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.failure.open", 1);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.last_error", error);
        qWarning("Failed to open the Windows clipboard");
        return false;
    }

    bool emptied = false;
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.empty");
        emptied = EmptyClipboard() != FALSE;
    }
    DWORD clipboardError = ERROR_SUCCESS;
    if (!emptied) {
        clipboardError = GetLastError();
    }

    bool published = false;
    if (emptied) {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.set_data");
        published = SetClipboardData(CF_DIBV5, allocation) != nullptr;
        if (!published) {
            clipboardError = GetLastError();
        }
    }
    {
        SNOW_SHOT_CLIPBOARD_PERF_SCOPE("clipboard.close");
        CloseClipboard();
    }
    if (!published) {
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER(
            emptied ? "clipboard.failure.set_data" : "clipboard.failure.empty", 1);
        SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.last_error", clipboardError);
        qWarning("Failed to publish CF_DIBV5 clipboard image");
        return false;
    }

    *handle = nullptr;
    SNOW_SHOT_CLIPBOARD_PERF_COUNTER("clipboard.success", 1);
    return true;
}
} // namespace
#endif

ScreenshotClipboardPayload::~ScreenshotClipboardPayload() {
    reset();
}

void ScreenshotClipboardPayload::reset() noexcept {
#if defined(Q_OS_WIN) || defined(_WIN32)
    if (m_nativeHandle != nullptr) {
        GlobalFree(static_cast<HGLOBAL>(m_nativeHandle));
        m_nativeHandle = nullptr;
    }
#else
    m_image = {};
#endif
}

ScreenshotClipboardPayload::ScreenshotClipboardPayload(ScreenshotClipboardPayload&& other) noexcept {
#if defined(Q_OS_WIN) || defined(_WIN32)
    m_nativeHandle = other.m_nativeHandle;
    other.m_nativeHandle = nullptr;
#else
    m_image = std::move(other.m_image);
#endif
}

ScreenshotClipboardPayload& ScreenshotClipboardPayload::operator=(
    ScreenshotClipboardPayload&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    reset();
#if defined(Q_OS_WIN) || defined(_WIN32)
    m_nativeHandle = other.m_nativeHandle;
    other.m_nativeHandle = nullptr;
#else
    m_image = std::move(other.m_image);
#endif
    return *this;
}

bool ScreenshotClipboardPayload::isValid() const {
#if defined(Q_OS_WIN) || defined(_WIN32)
    return m_nativeHandle != nullptr;
#else
    return !m_image.isNull();
#endif
}

ScreenshotClipboardPayload ScreenshotClipboardService::prepare(
    ScreenshotClipboardPixelSource source) {
    if (!source.isValid()) {
        qWarning("Ignoring null screenshot clipboard image");
        return {};
    }
#if defined(Q_OS_WIN) || defined(_WIN32)
    ScreenshotClipboardPayload payload;
    payload.m_nativeHandle = prepareDibV5(source);
    return payload;
#else
    ScreenshotClipboardPayload payload;
    payload.m_image = source.image();
    return payload;
#endif
}

ScreenshotClipboardPayload ScreenshotClipboardService::prepareImage(const QImage& image) {
    return prepare(ScreenshotClipboardPixelSource(image));
}

bool ScreenshotClipboardService::publish(QClipboard* clipboard, ScreenshotClipboardPayload payload) {
#if defined(Q_OS_WIN) || defined(_WIN32)
    Q_UNUSED(clipboard);
    return publishDibV5(&payload.m_nativeHandle);
#else
    if (clipboard == nullptr || !payload.isValid()) {
        qWarning("Screenshot clipboard is unavailable");
        return false;
    }
    clipboard->setImage(payload.m_image, QClipboard::Clipboard);
    return true;
#endif
}

bool ScreenshotClipboardService::publishImage(QClipboard* clipboard, const QImage& image) {
    return publish(clipboard, prepareImage(image));
}
