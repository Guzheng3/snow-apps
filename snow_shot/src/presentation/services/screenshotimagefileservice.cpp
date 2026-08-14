#include "snow_shot/presentation/screenshotimagefileservice.h"

#include "snowimageqtcodec.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {
QString filterForFormat(ScreenshotImageFileFormat format) {
    switch (format) {
    case ScreenshotImageFileFormat::Png:
        return QCoreApplication::translate("ScreenshotImageFileService", "PNG image (*.png)");
    case ScreenshotImageFileFormat::Jpeg:
        return QCoreApplication::translate("ScreenshotImageFileService",
                                           "JPEG image (*.jpg *.jpeg)");
    case ScreenshotImageFileFormat::Webp:
        return QCoreApplication::translate("ScreenshotImageFileService", "WebP image (*.webp)");
    case ScreenshotImageFileFormat::Jxl:
        return QCoreApplication::translate("ScreenshotImageFileService", "JPEG XL image (*.jxl)");
    case ScreenshotImageFileFormat::Avif:
        return QCoreApplication::translate("ScreenshotImageFileService", "AVIF image (*.avif)");
    }
    return {};
}

QString collisionSafePath(const QString& directory, const QString& baseName,
                          const QString& extension) {
    const QDir target(directory);
    QString candidate = target.filePath(QStringLiteral("%1.%2").arg(baseName, extension));
    for (int suffix = 1; QFileInfo::exists(candidate); ++suffix) {
        candidate = target.filePath(
            QStringLiteral("%1_%2.%3").arg(baseName).arg(suffix).arg(extension));
    }
    return candidate;
}
} // namespace

QString ScreenshotImageFileService::suggestedBaseName(const QDateTime& timestamp) {
    return QStringLiteral("SnowShot_%1").arg(
        timestamp.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
}

QString ScreenshotImageFileService::dialogFilter(ScreenshotImageFileFormat format) {
    return filterForFormat(format);
}

QString ScreenshotImageFileService::saveDialogFilter() {
    return QStringList{filterForFormat(ScreenshotImageFileFormat::Png),
                       filterForFormat(ScreenshotImageFileFormat::Jpeg),
                       filterForFormat(ScreenshotImageFileFormat::Webp),
                       filterForFormat(ScreenshotImageFileFormat::Jxl),
                       filterForFormat(ScreenshotImageFileFormat::Avif)}
        .join(QStringLiteral(";;"));
}

QString ScreenshotImageFileService::automaticDirectory() {
    const QStringList directories = automaticDirectories();
    return directories.isEmpty() ? QString() : directories.constFirst();
}

QStringList ScreenshotImageFileService::automaticDirectories() {
    QStringList directories;
    for (QStandardPaths::StandardLocation location :
         {QStandardPaths::PicturesLocation, QStandardPaths::DocumentsLocation}) {
        const QString root = QStandardPaths::writableLocation(location);
        if (root.isEmpty()) {
            continue;
        }
        const QString directory = QDir(root).filePath(QStringLiteral("SnowShot"));
        if (!directories.contains(directory, Qt::CaseInsensitive)) {
            directories.push_back(directory);
        }
    }
    return directories;
}

QString ScreenshotImageFileService::extension(ScreenshotImageFileFormat format) {
    switch (format) {
    case ScreenshotImageFileFormat::Png:
        return QStringLiteral("png");
    case ScreenshotImageFileFormat::Jpeg:
        return QStringLiteral("jpg");
    case ScreenshotImageFileFormat::Webp:
        return QStringLiteral("webp");
    case ScreenshotImageFileFormat::Jxl:
        return QStringLiteral("jxl");
    case ScreenshotImageFileFormat::Avif:
        return QStringLiteral("avif");
    }
    return {};
}

QString ScreenshotImageFileService::normalizedPath(QString path,
                                                   ScreenshotImageFileFormat format) {
    path = QDir::cleanPath(path.trimmed());
    if (path.isEmpty()) {
        return {};
    }
    const QFileInfo information(path);
    const QString fileName = information.fileName();
    const qsizetype dot = fileName.lastIndexOf(QLatin1Char('.'));
    const bool hasExplicitSuffix = dot >= 0 && dot + 1 < fileName.size();
    // The format passed to write() describes the bytes being emitted. Always
    // make the filename agree with it; callers that need suffix inference do
    // that once, through formatForDialogSelection(), before writing.
    if (hasExplicitSuffix && dot > 0) {
        path.chop(fileName.size() - dot);
    } else if (path.endsWith(QLatin1Char('.'))) {
        path.chop(1);
    }
    return path + QStringLiteral(".") + extension(format);
}

std::optional<ScreenshotImageFileFormat>
ScreenshotImageFileService::formatForPath(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("png")) {
        return ScreenshotImageFileFormat::Png;
    }
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        return ScreenshotImageFileFormat::Jpeg;
    }
    if (suffix == QStringLiteral("webp")) {
        return ScreenshotImageFileFormat::Webp;
    }
    if (suffix == QStringLiteral("jxl")) {
        return ScreenshotImageFileFormat::Jxl;
    }
    if (suffix == QStringLiteral("avif")) {
        return ScreenshotImageFileFormat::Avif;
    }
    return std::nullopt;
}

ScreenshotImageFileFormat ScreenshotImageFileService::formatForDialogSelection(
    const QString& path, const QString& selectedFilter) {
    if (const auto fromPath = formatForPath(path); fromPath.has_value()) {
        return *fromPath;
    }
    for (ScreenshotImageFileFormat format :
         {ScreenshotImageFileFormat::Png, ScreenshotImageFileFormat::Jpeg,
          ScreenshotImageFileFormat::Webp, ScreenshotImageFileFormat::Jxl,
          ScreenshotImageFileFormat::Avif}) {
        if (selectedFilter == filterForFormat(format)) {
            return format;
        }
    }
    return ScreenshotImageFileFormat::Png;
}

snow::image::Format ScreenshotImageFileService::snowImageFormat(
    ScreenshotImageFileFormat format) {
    switch (format) {
    case ScreenshotImageFileFormat::Png:
        return snow::image::Format::png;
    case ScreenshotImageFileFormat::Jpeg:
        return snow::image::Format::jpeg;
    case ScreenshotImageFileFormat::Webp:
        return snow::image::Format::webp;
    case ScreenshotImageFileFormat::Jxl:
        return snow::image::Format::jxl;
    case ScreenshotImageFileFormat::Avif:
        return snow::image::Format::avif;
    }
    return snow::image::Format::unknown;
}

snow::image::EncodeOptions ScreenshotImageFileService::encodeOptions(
    ScreenshotImageFileFormat format) {
    snow::image::EncodeOptions options;
    options.format = snowImageFormat(format);
    options.preserve_metadata = false;
    switch (format) {
    case ScreenshotImageFileFormat::Png:
        options.compression_level = 0;
        break;
    case ScreenshotImageFileFormat::Jpeg:
        options.quality = 100;
        break;
    case ScreenshotImageFileFormat::Webp:
        options.lossless = true;
        options.lossless_effort = 0;
        break;
    case ScreenshotImageFileFormat::Jxl:
    case ScreenshotImageFileFormat::Avif:
        options.lossless = true;
        options.effort = 1;
        break;
    }
    return options;
}

ScreenshotImageFileSaveResult ScreenshotImageFileService::write(
    const QImage& image, const QString& path, ScreenshotImageFileFormat format) {
    if (image.isNull()) {
        return {{}, QStringLiteral("The screenshot image is empty")};
    }
    const QString outputPath = normalizedPath(path, format);
    if (outputPath.isEmpty()) {
        return {{}, QStringLiteral("No output file was selected")};
    }

    QString encodeError;
    const QByteArray encoded = snow_shot::image_codec::encode(
        image, snowImageFormat(format), encodeOptions(format), &encodeError);
    if (encoded.isEmpty()) {
        return {{}, encodeError.isEmpty() ? QStringLiteral("The image could not be encoded")
                                          : encodeError};
    }

    QSaveFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {{}, file.errorString()};
    }
    if (file.write(encoded) != encoded.size()) {
        return {{}, file.errorString()};
    }
    if (!file.commit()) {
        return {{}, file.errorString()};
    }
    return {outputPath, {}};
}

ScreenshotImageFileSaveResult ScreenshotImageFileService::saveAutomatically(
    const QImage& image) {
    return saveAutomatically(image, automaticDirectories(), QDateTime::currentDateTime());
}

ScreenshotImageFileSaveResult ScreenshotImageFileService::saveAutomatically(
    const QImage& image, const QStringList& candidateDirectories, const QDateTime& timestamp) {
    if (image.isNull()) {
        return {{}, QStringLiteral("The screenshot image is empty")};
    }

    QString lastError = QStringLiteral("No automatic screenshot folder is available");
    for (const QString& candidate : candidateDirectories) {
        const QString trimmedCandidate = candidate.trimmed();
        if (trimmedCandidate.isEmpty()) {
            continue;
        }
        const QString directory = QDir::cleanPath(trimmedCandidate);
        if (!QDir().mkpath(directory)) {
            lastError = QStringLiteral("The screenshot folder could not be created: %1")
                            .arg(directory);
            continue;
        }

        const QString path = collisionSafePath(directory, suggestedBaseName(timestamp),
                                               extension(ScreenshotImageFileFormat::Png));
        const ScreenshotImageFileSaveResult result =
            write(image, path, ScreenshotImageFileFormat::Png);
        if (result.succeeded()) {
            return result;
        }
        lastError = result.error;
    }
    return {{}, lastError};
}

bool ScreenshotImageFileService::publishFileToClipboard(QClipboard* clipboard,
                                                        const QString& path) {
    if (clipboard == nullptr || path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    auto* mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath())});
    clipboard->setMimeData(mimeData, QClipboard::Clipboard);
    return true;
}
