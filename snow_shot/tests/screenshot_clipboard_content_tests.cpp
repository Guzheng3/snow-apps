#include "snow_shot/presentation/screenshotclipboardcontent.h"

#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QUrl>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

QByteArray pngBytes(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    require(buffer.open(QIODevice::WriteOnly), "PNG buffer should open");
    require(image.save(&buffer, "PNG"), "PNG image should encode");
    return bytes;
}

void directImageWinsOverRichText() {
    QMimeData mime;
    QImage image(QSize(3, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    mime.setImageData(image);
    mime.setHtml(QStringLiteral("<b>ignored</b>"));

    const auto content = ScreenshotClipboardContentReader::readMimeData(&mime);
    require(content.has_value() && !content->isFormattedText() &&
                content->image.size() == QSize(3, 2),
            "direct clipboard images should have priority over HTML");
}

void oversizedDirectImagesAreIgnored() {
    QMimeData mime;
    QImage image(QSize(8192, 8192), QImage::Format_Mono);
    require(!image.isNull(), "oversized clipboard fixture should allocate");
    mime.setImageData(image);
    require(!ScreenshotClipboardContentReader::readMimeData(&mime).has_value(),
            "oversized direct clipboard images should be ignored");
}

void encodedImageAndTextAreSupported() {
    QMimeData mime;
    QImage image(QSize(4, 5), QImage::Format_RGBA8888);
    image.fill(Qt::blue);
    mime.setData(QStringLiteral("image/png"), pngBytes(image));

    const auto imageContent = ScreenshotClipboardContentReader::readMimeData(&mime);
    require(imageContent.has_value() && !imageContent->isFormattedText() &&
                imageContent->image.size() == QSize(4, 5),
            "encoded PNG clipboard data should decode");

    QMimeData textMime;
    const QString longText(4096, u'W');
    textMime.setText(longText);
    const auto textContent = ScreenshotClipboardContentReader::readMimeData(&textMime);
    require(textContent.has_value() && textContent->isFormattedText() &&
                textContent->formattedDocument != nullptr &&
                textContent->plainText == longText && textContent->image.width() == 1024 &&
                !textContent->image.isNull(),
            "plain clipboard text should be rendered with a bounded image");
}

void localImageFilesAndPlainTextFallbackAreSupported() {
    QTemporaryDir directory;
    require(directory.isValid(), "temporary image directory should be available");
    const QString path = directory.filePath(QStringLiteral("clipboard.png"));
    QImage image(QSize(7, 6), QImage::Format_RGBA8888);
    image.fill(Qt::green);
    require(image.save(path, "PNG"), "temporary clipboard image should encode");

    QMimeData fileMime;
    fileMime.setUrls({QUrl::fromLocalFile(path)});
    const auto fileContent = ScreenshotClipboardContentReader::readMimeData(&fileMime);
    require(fileContent.has_value() && !fileContent->isFormattedText() &&
                fileContent->image.size() == QSize(7, 6),
            "supported local image-file URLs should decode");

    QMimeData fallbackMime;
    fallbackMime.setHtml(QString());
    fallbackMime.setText(QStringLiteral("plain fallback"));
    const auto fallbackContent = ScreenshotClipboardContentReader::readMimeData(&fallbackMime);
    require(fallbackContent.has_value() && fallbackContent->isFormattedText() &&
                fallbackContent->plainText == QStringLiteral("plain fallback"),
            "plain text should be used when an advertised HTML payload is empty");
}

void htmlIsSelectableAndExternalResourcesAreBlocked() {
    QMimeData mime;
    mime.setHtml(QStringLiteral(
        "<p><b>Bold</b> and <i>italic</i></p><img src=\"https://example.invalid/a.png\">"));
    const auto content = ScreenshotClipboardContentReader::readMimeData(&mime);
    require(content.has_value() && content->isFormattedText() &&
                content->formattedDocument != nullptr && content->image.width() <= 1024,
            "HTML clipboard data should produce a formatted document and raster");
    bool foundBold = false;
    for (QTextBlock block = content->formattedDocument->begin(); block.isValid();
         block = block.next()) {
        for (QTextBlock::iterator iterator = block.begin(); !iterator.atEnd(); ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid() && fragment.text().contains(QStringLiteral("Bold"))) {
                foundBold = fragment.charFormat().fontWeight() >= QFont::Bold;
            }
        }
    }
    require(foundBold, "HTML formatting should be retained by the Qt document");
    require(!content->formattedDocument->resource(QTextDocument::ImageResource,
                                                  QUrl(QStringLiteral("https://example.invalid/a.png")))
                 .isValid(),
            "external HTML resources must not be fetched");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    directImageWinsOverRichText();
    oversizedDirectImagesAreIgnored();
    encodedImageAndTextAreSupported();
    localImageFilesAndPlainTextFallbackAreSupported();
    htmlIsSelectableAndExternalResourcesAreBlocked();
    return 0;
}
