#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDCONTENT_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDCONTENT_H

#include <QImage>
#include <QString>

#include <memory>
#include <optional>

class QClipboard;
class QMimeData;
class QTextDocument;

enum class ScreenshotClipboardContentKind {
    Image,
    FormattedText,
};

struct ScreenshotClipboardContent {
    ScreenshotClipboardContentKind kind = ScreenshotClipboardContentKind::Image;
    QImage image;
    std::shared_ptr<QTextDocument> formattedDocument;
    QString plainText;

    [[nodiscard]] bool isValid() const {
        return !image.isNull() && !image.size().isEmpty() &&
               (kind == ScreenshotClipboardContentKind::Image || formattedDocument != nullptr);
    }

    [[nodiscard]] bool isFormattedText() const {
        return kind == ScreenshotClipboardContentKind::FormattedText &&
               formattedDocument != nullptr;
    }
};

class ScreenshotClipboardContentReader final {
  public:
    // The clipboard and MIME data are read synchronously on the GUI thread. The
    // returned document and image are detached snapshots and do not retain the
    // QMimeData object.
    [[nodiscard]] static std::optional<ScreenshotClipboardContent> read(
        QClipboard* clipboard);
    [[nodiscard]] static std::optional<ScreenshotClipboardContent> readMimeData(
        const QMimeData* mimeData);
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCLIPBOARDCONTENT_H
