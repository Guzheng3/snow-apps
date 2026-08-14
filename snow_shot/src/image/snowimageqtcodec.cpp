#include "snowimageqtcodec.h"

#include "snowimagecodecbridge.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

#include <QFile>
#include <QPainter>

namespace snow_shot::image_codec {
namespace {

constexpr std::size_t kBackendErrorCapacity = 1024;

class BackendBuffer final {
  public:
    BackendBuffer() = default;
    ~BackendBuffer() {
        snow_shot_image_codec_release_buffer(&value);
    }

    BackendBuffer(const BackendBuffer&) = delete;
    BackendBuffer& operator=(const BackendBuffer&) = delete;

    SnowShotImageCodecBuffer value{};
};

bool backendAbiIsCompatible() noexcept {
    return snow_shot_image_codec_abi_version() == SNOW_SHOT_IMAGE_CODEC_ABI_VERSION;
}

uint32_t bridgeFormat(snow::image::Format format) noexcept {
    switch (format) {
    case snow::image::Format::unknown:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_UNKNOWN;
    case snow::image::Format::bmp:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_BMP;
    case snow::image::Format::cur:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_CUR;
    case snow::image::Format::gif:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_GIF;
    case snow::image::Format::ico:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_ICO;
    case snow::image::Format::jpeg:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_JPEG;
    case snow::image::Format::pbm:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_PBM;
    case snow::image::Format::pgm:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_PGM;
    case snow::image::Format::png:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_PNG;
    case snow::image::Format::ppm:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_PPM;
    case snow::image::Format::svg:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_SVG;
    case snow::image::Format::svgz:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_SVGZ;
    case snow::image::Format::xbm:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_XBM;
    case snow::image::Format::xpm:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_XPM;
    case snow::image::Format::heif:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_HEIF;
    case snow::image::Format::avif:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_AVIF;
    case snow::image::Format::jxl:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_JXL;
    case snow::image::Format::exr:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_EXR;
    case snow::image::Format::webp:
        return SNOW_SHOT_IMAGE_CODEC_FORMAT_WEBP;
    }
    return SNOW_SHOT_IMAGE_CODEC_FORMAT_UNKNOWN;
}

uint8_t bridgeChromaSubsampling(snow::image::ChromaSubsampling value) noexcept {
    switch (value) {
    case snow::image::ChromaSubsampling::none:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_NONE;
    case snow::image::ChromaSubsampling::yuv444:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV444;
    case snow::image::ChromaSubsampling::yuv422:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV422;
    case snow::image::ChromaSubsampling::yuv420:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV420;
    case snow::image::ChromaSubsampling::yuv440:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV440;
    case snow::image::ChromaSubsampling::yuv411:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV411;
    case snow::image::ChromaSubsampling::yuv441:
        return SNOW_SHOT_IMAGE_CODEC_CHROMA_YUV441;
    }
    return SNOW_SHOT_IMAGE_CODEC_CHROMA_NONE;
}

uint8_t bridgeAlphaContent(snow::image::AlphaContent value) noexcept {
    switch (value) {
    case snow::image::AlphaContent::opaque:
        return SNOW_SHOT_IMAGE_CODEC_ALPHA_OPAQUE;
    case snow::image::AlphaContent::non_opaque:
        return SNOW_SHOT_IMAGE_CODEC_ALPHA_NON_OPAQUE;
    }
    return SNOW_SHOT_IMAGE_CODEC_ALPHA_OPAQUE;
}

SnowShotImageCodecEncodeOptions bridgeOptions(snow::image::Format format,
                                              const snow::image::EncodeOptions& options) noexcept {
    SnowShotImageCodecEncodeOptions result{};
    result.struct_size = static_cast<uint32_t>(sizeof(SnowShotImageCodecEncodeOptions));
    result.abi_version = SNOW_SHOT_IMAGE_CODEC_ABI_VERSION;
    result.format = bridgeFormat(format);
    result.quality = options.quality;
    result.effort = options.effort;
    result.lossless_effort = options.lossless_effort;
    result.compression_level = options.compression_level;
    result.lossless = options.lossless ? uint8_t{1} : uint8_t{0};
    result.preserve_metadata = options.preserve_metadata ? uint8_t{1} : uint8_t{0};
    result.progressive = options.progressive ? uint8_t{1} : uint8_t{0};
    result.interlaced = options.interlaced ? uint8_t{1} : uint8_t{0};
    if (options.chroma_subsampling.has_value()) {
        result.has_chroma_subsampling = 1;
        result.chroma_subsampling = bridgeChromaSubsampling(*options.chroma_subsampling);
    }
    if (options.verified_alpha_content.has_value()) {
        result.has_verified_alpha_content = 1;
        result.verified_alpha_content = bridgeAlphaContent(*options.verified_alpha_content);
    }
    return result;
}

void setError(QString* output, const char* backendError, const char* fallback) {
    if (output == nullptr) {
        return;
    }
    *output = backendError != nullptr && backendError[0] != '\0' ? QString::fromUtf8(backendError)
                                                                 : QString::fromLatin1(fallback);
}

QImage prepareForEncoding(const QImage& source, snow::image::Format format) {
    if (format != snow::image::Format::jpeg || !source.hasAlphaChannel()) {
        return source;
    }
    QImage flattened(source.size(), QImage::Format_RGBX8888);
    flattened.fill(Qt::white);
    QPainter painter(&flattened);
    painter.drawImage(QPoint(), source);
    return flattened;
}

QByteArray encodeImage(const QImage& image, snow::image::Format format,
                       const snow::image::EncodeOptions& options, QString* error) {
    if (error != nullptr) {
        error->clear();
    }
    const uint32_t encodedFormat = bridgeFormat(format);
    if (!backendAbiIsCompatible()) {
        setError(error, nullptr, "The image codec backend is incompatible.");
        return {};
    }
    if (image.isNull() || encodedFormat == SNOW_SHOT_IMAGE_CODEC_FORMAT_UNKNOWN) {
        setError(error, nullptr, "Cannot encode an empty image or unknown image format.");
        return {};
    }

    const QImage rgba = prepareForEncoding(image, format).convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull() || rgba.width() <= 0 || rgba.height() <= 0 || rgba.bytesPerLine() <= 0 ||
        rgba.sizeInBytes() <= 0) {
        setError(error, nullptr, "The image could not be converted to RGBA pixels.");
        return {};
    }

    const SnowShotImageCodecEncodeOptions encodedOptions = bridgeOptions(format, options);
    BackendBuffer output;
    std::array<char, kBackendErrorCapacity> backendError{};
    const int32_t succeeded = snow_shot_image_codec_encode_rgba8(
        rgba.constBits(), static_cast<uint64_t>(rgba.sizeInBytes()),
        static_cast<uint32_t>(rgba.width()), static_cast<uint32_t>(rgba.height()),
        static_cast<uint64_t>(rgba.bytesPerLine()), &encodedOptions, &output.value,
        backendError.data(), static_cast<uint64_t>(backendError.size()));
    if (succeeded == 0 || output.value.data == nullptr || output.value.size == 0) {
        setError(error, backendError.data(), "Image encoding failed.");
        return {};
    }
    if (output.value.size > static_cast<uint64_t>(std::numeric_limits<qsizetype>::max())) {
        setError(error, nullptr, "The encoded image is too large for Qt.");
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(output.value.data),
                      static_cast<qsizetype>(output.value.size));
}

QImage decodeBytes(const QByteArray& encoded, snow::image::Format expectedFormat) {
    const uint32_t bridgeExpectedFormat = bridgeFormat(expectedFormat);
    if (!backendAbiIsCompatible() || encoded.isEmpty() ||
        bridgeExpectedFormat == SNOW_SHOT_IMAGE_CODEC_FORMAT_UNKNOWN) {
        return {};
    }

    BackendBuffer output;
    std::array<char, kBackendErrorCapacity> backendError{};
    const int32_t succeeded = snow_shot_image_codec_decode_rgba8(
        reinterpret_cast<const uint8_t*>(encoded.constData()),
        static_cast<uint64_t>(encoded.size()), bridgeExpectedFormat, &output.value,
        backendError.data(), static_cast<uint64_t>(backendError.size()));
    if (succeeded == 0 || output.value.data == nullptr || output.value.width == 0 ||
        output.value.height == 0 ||
        output.value.width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        output.value.height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    const uint64_t rowBytes = static_cast<uint64_t>(output.value.width) * 4U;
    if (output.value.row_stride != rowBytes ||
        output.value.height > std::numeric_limits<uint64_t>::max() / rowBytes) {
        return {};
    }
    const uint64_t requiredSize = rowBytes * output.value.height;
    if (requiredSize > output.value.size ||
        requiredSize > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return {};
    }

    QImage result(static_cast<int>(output.value.width), static_cast<int>(output.value.height),
                  QImage::Format_RGBA8888);
    if (result.isNull() || static_cast<uint64_t>(result.bytesPerLine()) < rowBytes) {
        return {};
    }
    const std::size_t sourceStride = static_cast<std::size_t>(rowBytes);
    for (uint32_t row = 0; row < output.value.height; ++row) {
        std::memcpy(result.scanLine(static_cast<int>(row)),
                    output.value.data + static_cast<std::size_t>(row) * sourceStride, sourceStride);
    }
    return result;
}

bool inspectBytes(const QByteArray& encoded, snow::image::Format expectedFormat,
                  const QSize& expectedSize) {
    const uint32_t bridgeExpectedFormat = bridgeFormat(expectedFormat);
    if (!backendAbiIsCompatible() || encoded.isEmpty() || !expectedSize.isValid() ||
        bridgeExpectedFormat == SNOW_SHOT_IMAGE_CODEC_FORMAT_UNKNOWN) {
        return false;
    }

    SnowShotImageCodecImageInfo information{};
    std::array<char, kBackendErrorCapacity> backendError{};
    const int32_t succeeded = snow_shot_image_codec_inspect(
        reinterpret_cast<const uint8_t*>(encoded.constData()),
        static_cast<uint64_t>(encoded.size()), bridgeExpectedFormat, &information,
        backendError.data(), static_cast<uint64_t>(backendError.size()));
    return succeeded != 0 && information.width == static_cast<uint32_t>(expectedSize.width()) &&
           information.height == static_cast<uint32_t>(expectedSize.height());
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

} // namespace

QByteArray encode(const QImage& image, snow::image::Format format,
                  const snow::image::EncodeOptions& options, QString* error) {
    return encodeImage(image, format, options, error);
}

QByteArray encodePng(const QImage& image) {
    snow::image::EncodeOptions options;
    options.compression_level = 1;
    return encodeImage(image, snow::image::Format::png, options, nullptr);
}

QByteArray encodeWebp(const QImage& image, int quality) {
    snow::image::EncodeOptions options;
    options.quality = quality;
    return encodeImage(image, snow::image::Format::webp, options, nullptr);
}

QImage decode(const QByteArray& encoded, snow::image::Format expectedFormat,
              const char* /*nameHint*/) {
    return decodeBytes(encoded, expectedFormat);
}

QImage decodeFile(const QString& path, snow::image::Format expectedFormat) {
    return decodeBytes(readFile(path), expectedFormat);
}

bool inspect(const QByteArray& encoded, snow::image::Format expectedFormat,
             const QSize& expectedSize, const char* /*nameHint*/) {
    return inspectBytes(encoded, expectedFormat, expectedSize);
}

bool inspectFile(const QString& path, snow::image::Format expectedFormat,
                 const QSize& expectedSize) {
    return inspectBytes(readFile(path), expectedFormat, expectedSize);
}

} // namespace snow_shot::image_codec
