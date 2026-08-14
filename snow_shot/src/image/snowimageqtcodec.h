#pragma once

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

#include <snow/image/codec.h>
#include <snow/image/format.h>

namespace snow_shot::image_codec {

[[nodiscard]] QByteArray encode(const QImage& image, snow::image::Format format,
                                const snow::image::EncodeOptions& options,
                                QString* error = nullptr);
[[nodiscard]] QByteArray encodePng(const QImage& image);
[[nodiscard]] QByteArray encodeWebp(const QImage& image, int quality = 75);
[[nodiscard]] QImage decode(const QByteArray& encoded, snow::image::Format expectedFormat,
                            const char* nameHint);
[[nodiscard]] QImage decodeFile(const QString& path, snow::image::Format expectedFormat);
[[nodiscard]] bool inspect(const QByteArray& encoded, snow::image::Format expectedFormat,
                           const QSize& expectedSize, const char* nameHint);
[[nodiscard]] bool inspectFile(const QString& path, snow::image::Format expectedFormat,
                               const QSize& expectedSize);

} // namespace snow_shot::image_codec
