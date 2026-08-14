#include "snow_shot/presentation/screenshotclipboardcontent.h"

#include "../../image/snowimageqtcodec.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QTextDocument>
#include <QTextOption>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>

#include <snow/image/format.h>

namespace {
constexpr int kMaximumRichTextWidth = 1024;
constexpr int kMaximumRichTextHeight = 32768;
constexpr qint64 kMaximumRichTextPixels = 64LL * 1024LL * 1024LL;
constexpr qint64 kMaximumClipboardImagePixels = 64LL * 1000LL * 1000LL;
constexpr qint64 kMaximumClipboardImageBytes = 256LL * 1024LL * 1024LL;

class RestrictedTextDocument final : public QTextDocument {
  public:
    QVariant loadResource(int type, const QUrl& name) override {
        if (!name.isValid() || name.isEmpty()) {
            return {};
        }

        const QString scheme = name.scheme().toLower();
        // Data URLs are self-contained. Every other scheme is denied before
        // Qt can perform I/O. The clipboard snapshot does not inject any
        // external resources, so an empty QVariant is the safe cache miss.
        if (scheme == QStringLiteral("data")) {
            return QTextDocument::loadResource(type, name);
        }
        return {};
    }
};

struct EncodedImageFormat {
    const char* mimeType;
    snow::image::Format format;
};

constexpr EncodedImageFormat kEncodedImageFormats[] = {
    {"image/png", snow::image::Format::png},
    {"image/jpeg", snow::image::Format::jpeg},
    {"image/jpg", snow::image::Format::jpeg},
    {"image/webp", snow::image::Format::webp},
    {"image/jxl", snow::image::Format::jxl},
    {"image/avif", snow::image::Format::avif},
};

struct FileImageFormat {
    const char* suffix;
    snow::image::Format format;
};

constexpr FileImageFormat kFileImageFormats[] = {
    {"png", snow::image::Format::png},
    {"jpg", snow::image::Format::jpeg},
    {"jpeg", snow::image::Format::jpeg},
    {"webp", snow::image::Format::webp},
    {"jxl", snow::image::Format::jxl},
    {"avif", snow::image::Format::avif},
};

QImage normalizedImage(QImage image) {
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }
    const qint64 pixels = static_cast<qint64>(image.width()) * image.height();
    if (pixels <= 0 || pixels > kMaximumClipboardImagePixels ||
        image.sizeInBytes() <= 0 || image.sizeInBytes() > kMaximumClipboardImageBytes) {
        return {};
    }
    image.setDevicePixelRatio(1.0);
    return image;
}

std::optional<ScreenshotClipboardContent> imageContent(QImage image) {
    image = normalizedImage(std::move(image));
    if (image.isNull()) {
        return std::nullopt;
    }
    ScreenshotClipboardContent result;
    result.kind = ScreenshotClipboardContentKind::Image;
    result.image = std::move(image);
    return result;
}

std::optional<ScreenshotClipboardContent> renderTextDocument(
    std::shared_ptr<QTextDocument> document, QString plainText) {
    if (document == nullptr) {
        return std::nullopt;
    }

    QTextOption option = document->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document->setDefaultTextOption(option);
    document->setDocumentMargin(0.0);
    document->setTextWidth(-1.0);

    qreal idealWidth = document->idealWidth();
    if (!std::isfinite(idealWidth) || idealWidth <= 0.0) {
        idealWidth = document->documentLayout()->documentSize().width();
    }
    if (!std::isfinite(idealWidth) || idealWidth <= 0.0) {
        idealWidth = 1.0;
    }
    const int width = std::clamp(qCeil(idealWidth), 1, kMaximumRichTextWidth);
    document->setTextWidth(width);

    const QSizeF documentSize = document->documentLayout()->documentSize();
    if (!documentSize.isValid() || documentSize.isEmpty() ||
        !std::isfinite(documentSize.height())) {
        return std::nullopt;
    }
    const int height = qCeil(documentSize.height());
    if (height <= 0 || height > kMaximumRichTextHeight ||
        static_cast<qint64>(width) * static_cast<qint64>(height) > kMaximumRichTextPixels) {
        return std::nullopt;
    }

    QImage image(QSize(width, height), QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return std::nullopt;
    }
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    document->drawContents(&painter, QRectF(0.0, 0.0, width, height));
    painter.end();

    ScreenshotClipboardContent result;
    result.kind = ScreenshotClipboardContentKind::FormattedText;
    result.image = std::move(image);
    result.formattedDocument = std::move(document);
    result.plainText = std::move(plainText);
    return result;
}

std::shared_ptr<QTextDocument> makeDocument(const QString& source, bool html,
                                            QString* plainText) {
    if (plainText == nullptr) {
        return {};
    }

    auto document = std::make_shared<RestrictedTextDocument>();
    if (html) {
        if (source.trimmed().isEmpty()) {
            return {};
        }
        document->setHtml(source);
    } else {
        if (source.isEmpty()) {
            return {};
        }
        document->setPlainText(source);
    }

    *plainText = document->toPlainText();
    if (plainText->isEmpty() && document->characterCount() <= 1) {
        return {};
    }
    return document;
}

std::optional<ScreenshotClipboardContent> readEncodedImage(const QMimeData* mimeData) {
    for (const EncodedImageFormat& candidate : kEncodedImageFormats) {
        if (!mimeData->hasFormat(QLatin1String(candidate.mimeType))) {
            continue;
        }
        const QByteArray encoded = mimeData->data(QLatin1String(candidate.mimeType));
        if (encoded.isEmpty()) {
            continue;
        }
        if (QImage image = snow_shot::image_codec::decode(encoded, candidate.format,
                                                           candidate.mimeType);
            !image.isNull()) {
            return imageContent(std::move(image));
        }
    }
    return std::nullopt;
}

std::optional<ScreenshotClipboardContent> readFileImage(const QMimeData* mimeData) {
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QFileInfo fileInfo(url.toLocalFile());
        const QString suffix = fileInfo.suffix().toLower();
        const auto format = std::find_if(std::begin(kFileImageFormats),
                                         std::end(kFileImageFormats),
                                         [&suffix](const FileImageFormat& candidate) {
                                             return suffix == QLatin1String(candidate.suffix);
                                         });
        if (format == std::end(kFileImageFormats)) {
            continue;
        }

        // The first supported local image URL owns the file-image tier. A
        // corrupt or unreadable file falls through to HTML/plain text, but a
        // later URL is never silently substituted.
        if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
            return std::nullopt;
        }
        QImage image = snow_shot::image_codec::decodeFile(fileInfo.absoluteFilePath(),
                                                           format->format);
        return imageContent(std::move(image));
    }
    return std::nullopt;
}
} // namespace

std::optional<ScreenshotClipboardContent> ScreenshotClipboardContentReader::read(
    QClipboard* clipboard) {
    return clipboard == nullptr ? std::nullopt : readMimeData(clipboard->mimeData());
}

std::optional<ScreenshotClipboardContent> ScreenshotClipboardContentReader::readMimeData(
    const QMimeData* mimeData) {
    if (mimeData == nullptr) {
        return std::nullopt;
    }

    if (const QVariant imageValue = mimeData->imageData(); imageValue.isValid()) {
        if (imageValue.canConvert<QImage>()) {
            if (auto result = imageContent(imageValue.value<QImage>()); result.has_value()) {
                return result;
            }
        }
        if (imageValue.canConvert<QPixmap>()) {
            if (auto result = imageContent(imageValue.value<QPixmap>().toImage());
                result.has_value()) {
                return result;
            }
        }
    }

    if (auto result = readEncodedImage(mimeData); result.has_value()) {
        return result;
    }
    if (auto result = readFileImage(mimeData); result.has_value()) {
        return result;
    }

    if (mimeData->hasHtml()) {
        QString plainText;
        if (auto document = makeDocument(mimeData->html(), true, &plainText);
            document != nullptr) {
            if (auto result = renderTextDocument(std::move(document), std::move(plainText));
                result.has_value()) {
                return result;
            }
        }
    }

    if (mimeData->hasText()) {
        QString plainText;
        if (auto document = makeDocument(mimeData->text(), false, &plainText);
            document != nullptr) {
            if (auto result = renderTextDocument(std::move(document), std::move(plainText));
                result.has_value()) {
                return result;
            }
        }
    }
    return std::nullopt;
}
