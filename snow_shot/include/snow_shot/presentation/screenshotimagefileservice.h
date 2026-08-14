#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTIMAGEFILESERVICE_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTIMAGEFILESERVICE_H

#include <QDateTime>
#include <QImage>
#include <QString>
#include <QStringList>

#include <snow/image/codec.h>
#include <snow/image/format.h>

#include <optional>

class QClipboard;

enum class ScreenshotImageFileFormat {
    Png,
    Jpeg,
    Webp,
    Jxl,
    Avif,
};

struct ScreenshotImageFileSaveResult {
    QString path;
    QString error;

    [[nodiscard]] bool succeeded() const { return !path.isEmpty() && error.isEmpty(); }
};

class ScreenshotImageFileService final {
  public:
    [[nodiscard]] static QString suggestedBaseName(
        const QDateTime& timestamp = QDateTime::currentDateTime());
    [[nodiscard]] static QString dialogFilter(ScreenshotImageFileFormat format);
    [[nodiscard]] static QString saveDialogFilter();
    [[nodiscard]] static QString automaticDirectory();
    [[nodiscard]] static QStringList automaticDirectories();
    [[nodiscard]] static QString extension(ScreenshotImageFileFormat format);
    [[nodiscard]] static QString normalizedPath(QString path, ScreenshotImageFileFormat format);
    [[nodiscard]] static std::optional<ScreenshotImageFileFormat> formatForPath(
        const QString& path);
    [[nodiscard]] static ScreenshotImageFileFormat formatForDialogSelection(
        const QString& path, const QString& selectedFilter);
    [[nodiscard]] static snow::image::Format snowImageFormat(ScreenshotImageFileFormat format);
    [[nodiscard]] static snow::image::EncodeOptions encodeOptions(
        ScreenshotImageFileFormat format);

    [[nodiscard]] static ScreenshotImageFileSaveResult write(
        const QImage& image, const QString& path, ScreenshotImageFileFormat format);
    [[nodiscard]] static ScreenshotImageFileSaveResult saveAutomatically(const QImage& image);
    [[nodiscard]] static ScreenshotImageFileSaveResult saveAutomatically(
        const QImage& image, const QStringList& candidateDirectories,
        const QDateTime& timestamp);
    [[nodiscard]] static bool publishFileToClipboard(QClipboard* clipboard,
                                                     const QString& path);
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTIMAGEFILESERVICE_H
