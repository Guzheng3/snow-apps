#include "snowimageqtcodec.h"

#include <snow/image/image.h>
#include <snow/image/io.h>
#include <snow/image/service.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace snow_shot::image_codec {
namespace {

const snow::image::Service& service() {
    static const snow::image::Service instance;
    return instance;
}

snow::image::Result<snow::image::Document> documentForImage(const QImage& source,
                                                            snow::image::Format format) {
    if (source.isNull()) {
        return snow::image::Status::error(snow::image::ErrorCode::invalid_argument,
                                           "Cannot encode a null QImage.");
    }

    QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    auto owner = std::make_shared<QImage>(std::move(rgba));
    const auto* data = reinterpret_cast<const std::byte*>(owner->constBits());
    const std::span<const std::byte> pixels(data, static_cast<std::size_t>(owner->sizeInBytes()));
    snow::image::Result<snow::image::SharedPixelBuffer> storage =
        snow::image::SharedPixelBuffer::adopt(owner, pixels);
    if (!storage) {
        return storage.error();
    }
    snow::image::Result<snow::image::Image> image = snow::image::Image::adopt(
        static_cast<std::uint32_t>(owner->width()), static_cast<std::uint32_t>(owner->height()),
        snow::image::kRgba8, owner->bytesPerLine(), std::move(storage).value());
    if (!image) {
        return image.error();
    }

    snow::image::Document document;
    document.format = format;
    document.canvas_width = static_cast<std::uint32_t>(owner->width());
    document.canvas_height = static_cast<std::uint32_t>(owner->height());
    snow::image::Frame frame;
    frame.image = std::move(image).value();
    document.frames.push_back(std::move(frame));
    return document;
}

QByteArray encodeImage(const QImage& image, snow::image::Format format, const char* nameHint,
                       int quality, int compressionLevel) {
    snow::image::Result<snow::image::Document> document = documentForImage(image, format);
    if (!document) {
        return {};
    }

    auto encoded = std::make_shared<std::vector<std::byte>>();
    snow::image::EncodeOptions options;
    options.format = format;
    options.quality = quality;
    options.compression_level = compressionLevel;
    snow::image::Result<snow::image::EncodeResult> result =
        service().encode(document.value(), snow::image::memory_output(encoded, nameHint), options);
    if (!result || encoded->empty()) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(encoded->data()),
                      static_cast<qsizetype>(encoded->size()));
}

snow::image::DecodeOptions decodeOptions() {
    snow::image::DecodeOptions options;
    options.output_format = snow::image::kRgba8;
    options.raster_layout = snow::image::RasterLayoutPolicy::packed;
    return options;
}

std::shared_ptr<const std::vector<std::byte>> byteOwner(const QByteArray& encoded) {
    auto bytes = std::make_shared<std::vector<std::byte>>(encoded.size());
    if (!encoded.isEmpty()) {
        std::memcpy(bytes->data(), encoded.constData(), static_cast<std::size_t>(encoded.size()));
    }
    return bytes;
}

std::filesystem::path filesystemPath(const QString& path) {
#if defined(_WIN32)
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

QImage decodeInput(const snow::image::Input& input, snow::image::Format expectedFormat) {
    snow::image::DecodeOptions options = decodeOptions();
    snow::image::Result<snow::image::Document> document = service().decode(input, options);
    if (!document || document.value().format != expectedFormat || document.value().frames.empty()) {
        return {};
    }

    const snow::image::Image& source = document.value().frames.front().image;
    if (source.format() != snow::image::kRgba8 || source.width() == 0 || source.height() == 0) {
        return {};
    }
    QImage result(static_cast<int>(source.width()), static_cast<int>(source.height()),
                  QImage::Format_RGBA8888);
    if (result.isNull()) {
        return {};
    }
    const std::size_t rowBytes = static_cast<std::size_t>(source.width()) * 4U;
    for (std::uint32_t row = 0; row < source.height(); ++row) {
        std::memcpy(result.scanLine(static_cast<int>(row)),
                    source.pixels().data() + static_cast<std::size_t>(row) * source.row_stride(),
                    std::min(rowBytes, static_cast<std::size_t>(result.bytesPerLine())));
    }
    return result;
}

bool inspectInput(const snow::image::Input& input, snow::image::Format expectedFormat,
                  const QSize& expectedSize) {
    snow::image::Result<snow::image::DocumentInfo> information = service().inspect(input);
    if (!information || information.value().format != expectedFormat ||
        information.value().frames.size() != 1) {
        return false;
    }
    const snow::image::FrameInfo& frame = information.value().frames.front();
    return frame.width == static_cast<std::uint32_t>(expectedSize.width()) &&
           frame.height == static_cast<std::uint32_t>(expectedSize.height());
}

} // namespace

QByteArray encodePng(const QImage& image) {
    // Level 1 is zlib's Z_BEST_SPEED setting and keeps screenshot encoding latency low.
    return encodeImage(image, snow::image::Format::png, "snow-shot.png", 90, 1);
}

QByteArray encodeWebp(const QImage& image, int quality) {
    return encodeImage(image, snow::image::Format::webp, "snow-shot.webp", quality, 6);
}

QImage decode(const QByteArray& encoded, snow::image::Format expectedFormat,
              const char* nameHint) {
    if (encoded.isEmpty()) {
        return {};
    }
    const auto bytes = byteOwner(encoded);
    return decodeInput(snow::image::memory_input(bytes, nameHint), expectedFormat);
}

QImage decodeFile(const QString& path, snow::image::Format expectedFormat) {
    snow::image::Result<snow::image::Input> input = snow::image::file_input(filesystemPath(path));
    return input ? decodeInput(input.value(), expectedFormat) : QImage();
}

bool inspect(const QByteArray& encoded, snow::image::Format expectedFormat,
             const QSize& expectedSize, const char* nameHint) {
    if (encoded.isEmpty() || !expectedSize.isValid()) {
        return false;
    }
    const auto bytes = byteOwner(encoded);
    return inspectInput(snow::image::memory_input(bytes, nameHint), expectedFormat, expectedSize);
}

bool inspectFile(const QString& path, snow::image::Format expectedFormat,
                 const QSize& expectedSize) {
    if (path.isEmpty() || !expectedSize.isValid()) {
        return false;
    }
    snow::image::Result<snow::image::Input> input = snow::image::file_input(filesystemPath(path));
    return input && inspectInput(input.value(), expectedFormat, expectedSize);
}

} // namespace snow_shot::image_codec
