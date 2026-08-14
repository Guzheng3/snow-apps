#include "snow_shot/presentation/screenshotimagefileservice.h"

#include "snowimageqtcodec.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrl>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

QImage image() {
    QImage result(QSize(3, 2), QImage::Format_RGBA8888);
    result.setPixelColor(0, 0, QColor(255, 0, 0, 255));
    result.setPixelColor(1, 0, QColor(0, 255, 0, 255));
    result.setPixelColor(2, 0, QColor(0, 0, 255, 255));
    result.setPixelColor(0, 1, QColor(255, 255, 255, 255));
    result.setPixelColor(1, 1, QColor(0, 0, 0, 255));
    result.setPixelColor(2, 1, QColor(80, 100, 120, 255));
    return result;
}

void namingAndFormatSelection() {
    const QDateTime timestamp(QDate(2026, 8, 14), QTime(9, 7, 6), QTimeZone::UTC);
    require(ScreenshotImageFileService::suggestedBaseName(timestamp) ==
                QStringLiteral("SnowShot_2026-08-14_09-07-06"),
            "automatic screenshot names must use the documented timestamp format");
    require(ScreenshotImageFileService::extension(ScreenshotImageFileFormat::Jpeg) ==
                QStringLiteral("jpg"),
            "JPEG should use the canonical jpg extension");
    require(ScreenshotImageFileService::formatForDialogSelection(
                QStringLiteral("capture.unknown"), QStringLiteral("JPEG image (*.jpg *.jpeg)")) ==
                ScreenshotImageFileFormat::Jpeg,
            "an unrecognized suffix should defer to the selected save-dialog filter");
    require(ScreenshotImageFileService::normalizedPath(QStringLiteral("capture.unknown"),
                                                       ScreenshotImageFileFormat::Png) ==
                QStringLiteral("capture.png"),
            "unsupported suffixes must be replaced by the selected format extension");
    require(ScreenshotImageFileService::normalizedPath(QStringLiteral("capture.jpg"),
                                                       ScreenshotImageFileFormat::Png) ==
                QStringLiteral("capture.png"),
            "recognized suffixes must still agree with the requested output format");
    require(ScreenshotImageFileService::normalizedPath(QStringLiteral("capture"),
                                                       ScreenshotImageFileFormat::Webp) ==
                QStringLiteral("capture.webp"),
            "paths without a suffix must receive the selected format extension");
    require(ScreenshotImageFileService::normalizedPath(QStringLiteral(".capture"),
                                                       ScreenshotImageFileFormat::Png) ==
                QStringLiteral(".capture.png"),
            "dot-prefixed names must retain their stem when an extension is added");
}

void writesLosslessImageAndPreservesCollisionNames() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory could not be created");
    const QDateTime timestamp(QDate(2026, 8, 14), QTime(9, 7, 6), QTimeZone::UTC);
    const QString base = ScreenshotImageFileService::suggestedBaseName(timestamp);
    const QString firstPath = QDir(directory.path()).filePath(base + QStringLiteral(".png"));
    QFile collision(firstPath);
    require(collision.open(QIODevice::WriteOnly) && collision.write("existing") == 8,
            "collision fixture could not be created");
    collision.close();

    const ScreenshotImageFileSaveResult result = ScreenshotImageFileService::saveAutomatically(
        image(), QStringList{directory.path()}, timestamp);
    require(result.succeeded(), "automatic PNG save should succeed");
    require(result.path == QDir(directory.path()).filePath(base + QStringLiteral("_1.png")),
            "automatic saves must preserve existing files and add a numeric suffix");
    require(snow_shot::image_codec::inspectFile(result.path, snow::image::Format::png,
                                                QSize(3, 2)),
            "the automatic PNG must be encoded by snow_image");
}

void writesEveryAdvertisedFormat() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary format directory could not be created");

    for (const ScreenshotImageFileFormat format : {
             ScreenshotImageFileFormat::Png,
             ScreenshotImageFileFormat::Jpeg,
             ScreenshotImageFileFormat::Webp,
             ScreenshotImageFileFormat::Jxl,
             ScreenshotImageFileFormat::Avif,
         }) {
        const QString path = QDir(directory.path())
                                 .filePath(QStringLiteral("encoded.%1")
                                               .arg(ScreenshotImageFileService::extension(format)));
        const ScreenshotImageFileSaveResult result =
            ScreenshotImageFileService::write(image(), path, format);
        require(result.succeeded(), "an advertised Save As format could not be encoded");
        require(snow_shot::image_codec::inspectFile(
                    result.path, ScreenshotImageFileService::snowImageFormat(format), QSize(3, 2)),
                "an advertised Save As output could not be inspected by snow_image");
    }
}

void retriesNextDirectoryAndPublishesFileOnlyClipboardData() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory could not be created");
    const QString blockedPath = QDir(directory.path()).filePath(QStringLiteral("blocked"));
    QFile blocked(blockedPath);
    require(blocked.open(QIODevice::WriteOnly) && blocked.write("x") == 1,
            "blocked-directory fixture could not be created");
    blocked.close();
    const QString fallback = QDir(directory.path()).filePath(QStringLiteral("fallback"));

    const ScreenshotImageFileSaveResult result = ScreenshotImageFileService::saveAutomatically(
        image(), QStringList{blockedPath, fallback},
        QDateTime(QDate(2026, 8, 14), QTime(9, 7, 6), QTimeZone::UTC));
    require(result.succeeded() && result.path.startsWith(fallback),
            "automatic saving should retry the next candidate directory after a failure");

    QClipboard* clipboard = QGuiApplication::clipboard();
    require(ScreenshotImageFileService::publishFileToClipboard(clipboard, result.path),
            "file clipboard publication should succeed for an existing file");
    const QMimeData* mime = clipboard->mimeData();
    require(mime != nullptr && mime->urls().size() == 1 && mime->hasUrls() &&
                mime->urls().constFirst().isLocalFile() &&
                mime->urls().constFirst().toLocalFile() == QFileInfo(result.path).absoluteFilePath() &&
                !mime->hasImage(),
            "file clipboard mode must publish a local file URL without image data");
}

void codecOptionsUseFastLosslessAndMaximumJpegQuality() {
    const auto jpeg = ScreenshotImageFileService::encodeOptions(ScreenshotImageFileFormat::Jpeg);
    require(jpeg.format == snow::image::Format::jpeg && jpeg.quality == 100,
            "JPEG saves must use quality 100");
    const auto png = ScreenshotImageFileService::encodeOptions(ScreenshotImageFileFormat::Png);
    require(png.format == snow::image::Format::png && png.compression_level == 0,
            "PNG saves must use the fastest compression setting");
    const auto webp = ScreenshotImageFileService::encodeOptions(ScreenshotImageFileFormat::Webp);
    require(webp.lossless && webp.lossless_effort == 0,
            "lossless WebP saves must use the fastest lossless effort");
    const auto jxl = ScreenshotImageFileService::encodeOptions(ScreenshotImageFileFormat::Jxl);
    const auto avif = ScreenshotImageFileService::encodeOptions(ScreenshotImageFileFormat::Avif);
    require(jxl.lossless && jxl.effort == 1 && avif.lossless && avif.effort == 1,
            "lossless JXL and AVIF saves must use the fastest effort");
}
} // namespace

int main(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    try {
        namingAndFormatSelection();
        writesLosslessImageAndPreservesCollisionNames();
        writesEveryAdvertisedFormat();
        retriesNextDirectoryAndPublishesFileOnlyClipboardData();
        codecOptionsUseFastLosslessAndMaximumJpegQuality();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
