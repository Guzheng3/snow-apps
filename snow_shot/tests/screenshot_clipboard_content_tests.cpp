#include "snow_shot/presentation/screenshotclipboardcontent.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QMimeData>
#include <QPalette>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextFrame>
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

bool imageContainsColor(const QImage& image, const QColor& expected) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) == expected) {
                return true;
            }
        }
    }
    return false;
}

void directImageWinsOverRichText() {
    QMimeData mime;
    QImage image(QSize(3, 2), QImage::Format_RGBA8888);
    image.fill(Qt::red);
    mime.setImageData(image);
    mime.setHtml(QStringLiteral("<b>ignored</b>"));

    const auto content = ScreenshotClipboardContentReader::readMimeData(&mime, 1.0);
    require(content.has_value() && !content->isFormattedText() &&
                content->image.size() == QSize(3, 2),
            "direct clipboard images should have priority over HTML");
}

void oversizedDirectImagesAreIgnored() {
    QMimeData mime;
    QImage image(QSize(8192, 8192), QImage::Format_Mono);
    require(!image.isNull(), "oversized clipboard fixture should allocate");
    mime.setImageData(image);
    require(!ScreenshotClipboardContentReader::readMimeData(&mime, 1.0).has_value(),
            "oversized direct clipboard images should be ignored");
}

void encodedImageAndTextAreSupported() {
    QMimeData mime;
    QImage image(QSize(4, 5), QImage::Format_RGBA8888);
    image.fill(Qt::blue);
    mime.setData(QStringLiteral("image/png"), pngBytes(image));

    const auto imageContent = ScreenshotClipboardContentReader::readMimeData(&mime, 1.0);
    require(imageContent.has_value() && !imageContent->isFormattedText() &&
                imageContent->image.size() == QSize(4, 5),
            "encoded PNG clipboard data should decode");

    QMimeData textMime;
    const QString longText(4096, u'W');
    textMime.setText(longText);
    const auto textContent = ScreenshotClipboardContentReader::readMimeData(&textMime, 1.0);
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
    const auto fileContent = ScreenshotClipboardContentReader::readMimeData(&fileMime, 1.0);
    require(fileContent.has_value() && !fileContent->isFormattedText() &&
                fileContent->image.size() == QSize(7, 6),
            "supported local image-file URLs should decode");

    QMimeData fallbackMime;
    fallbackMime.setHtml(QString());
    fallbackMime.setText(QStringLiteral("plain fallback"));
    const auto fallbackContent =
        ScreenshotClipboardContentReader::readMimeData(&fallbackMime, 1.0);
    require(fallbackContent.has_value() && fallbackContent->isFormattedText() &&
                fallbackContent->plainText == QStringLiteral("plain fallback"),
            "plain text should be used when an advertised HTML payload is empty");
}

void htmlIsSelectableAndExternalResourcesAreBlocked() {
    QMimeData mime;
    mime.setHtml(QStringLiteral(
        "<p><b>Bold</b> and <i>italic</i></p><img src=\"https://example.invalid/a.png\">"));
    const auto content = ScreenshotClipboardContentReader::readMimeData(&mime, 1.0);
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

void formattedTextHasPaddingOnEverySide() {
    const auto requirePadding = [](const ScreenshotClipboardContent& content,
                                   const char* message) {
        constexpr qreal expectedPadding = 16.0;
        require(content.formattedDocument != nullptr, message);
        const QTextDocument& document = *content.formattedDocument;
        const QRectF blockBounds =
            document.documentLayout()->blockBoundingRect(document.begin());
        const QSizeF documentSize = document.documentLayout()->documentSize();
        require(qFuzzyCompare(document.documentMargin(), expectedPadding) &&
                    blockBounds.left() >= expectedPadding &&
                    blockBounds.top() >= expectedPadding &&
                    documentSize.width() - blockBounds.right() >= expectedPadding &&
                    documentSize.height() - blockBounds.bottom() >= expectedPadding,
                message);
    };

    QMimeData plainTextMime;
    plainTextMime.setText(QStringLiteral("Padded plain text"));
    const auto plainTextContent =
        ScreenshotClipboardContentReader::readMimeData(&plainTextMime, 1.0);
    require(plainTextContent.has_value(), "plain text padding fixture should render");
    requirePadding(*plainTextContent, "plain text should have padding on every side");

    QMimeData htmlMime;
    htmlMime.setHtml(QStringLiteral("<p style=\"margin: 0\"><b>Padded HTML</b></p>"));
    const auto htmlContent = ScreenshotClipboardContentReader::readMimeData(&htmlMime, 1.0);
    require(htmlContent.has_value(), "HTML padding fixture should render");
    requirePadding(*htmlContent, "HTML should have padding on every side");
}

void formattedTextBackgroundFollowsApplicationTheme() {
    const QPalette originalPalette = QApplication::palette();
    QPalette themedPalette = originalPalette;
    const QColor themedBackground(QStringLiteral("#19324a"));
    themedPalette.setColor(QPalette::Base, themedBackground);
    QApplication::setPalette(themedPalette);

    QMimeData plainTextMime;
    plainTextMime.setText(QStringLiteral("Themed plain text"));
    const auto plainTextContent =
        ScreenshotClipboardContentReader::readMimeData(&plainTextMime, 1.0);
    require(plainTextContent.has_value() &&
                imageContainsColor(plainTextContent->image, themedBackground),
            "plain text raster background should follow the application theme");

    QMimeData htmlMime;
    htmlMime.setHtml(QStringLiteral("<p><b>Themed</b> HTML text</p>"));
    const auto htmlContent = ScreenshotClipboardContentReader::readMimeData(&htmlMime, 1.0);
    require(htmlContent.has_value() &&
                imageContainsColor(htmlContent->image, themedBackground),
            "HTML raster background should follow the application theme");

    QApplication::setPalette(originalPalette);
}

void htmlSourceBackgroundOverridesApplicationTheme() {
    const QPalette originalPalette = QApplication::palette();
    QPalette themedPalette = originalPalette;
    const QColor themedBackground(QStringLiteral("#19324a"));
    themedPalette.setColor(QPalette::Base, themedBackground);
    QApplication::setPalette(themedPalette);

    const QColor bodyBackground(QStringLiteral("#f3d7a4"));
    QMimeData bodyMime;
    bodyMime.setHtml(QStringLiteral(
        "<html><head><style>body { background-color: #f3d7a4; }</style></head>"
        "<body><p>Body background</p></body></html>"));
    const auto bodyContent =
        ScreenshotClipboardContentReader::readMimeData(&bodyMime, 1.0);
    require(bodyContent.has_value() && bodyContent->formattedDocument != nullptr &&
                bodyContent->formattedDocument->rootFrame()->frameFormat().background().color() ==
                    bodyBackground &&
                imageContainsColor(bodyContent->image, bodyBackground),
            "an HTML body background should override the application theme");

    const QColor inlineBackground(QStringLiteral("#d6ebff"));
    QMimeData inlineMime;
    inlineMime.setHtml(QStringLiteral(
        "<p style=\"margin: 0\"><span style=\"background-color: #d6ebff\">"
        "Browser clipboard background</span></p>"));
    const auto inlineContent =
        ScreenshotClipboardContentReader::readMimeData(&inlineMime, 1.0);
    require(inlineContent.has_value() && inlineContent->formattedDocument != nullptr &&
                inlineContent->formattedDocument->rootFrame()->frameFormat().background().color() ==
                    inlineBackground &&
                inlineContent->image.pixelColor(0, inlineContent->image.height() - 1) ==
                    inlineBackground,
            "a uniform clipboard text background should fill the formatted-text canvas");

    QMimeData mixedMime;
    mixedMime.setHtml(QStringLiteral(
        "<p><span style=\"background-color: #ffff00\">Highlighted</span> plain</p>"));
    const auto mixedContent =
        ScreenshotClipboardContentReader::readMimeData(&mixedMime, 1.0);
    require(mixedContent.has_value() && mixedContent->formattedDocument != nullptr &&
                mixedContent->formattedDocument->rootFrame()
                        ->frameFormat()
                        .background()
                        .style() == Qt::NoBrush &&
                imageContainsColor(mixedContent->image, themedBackground),
            "a partial text highlight must not replace the application canvas background");

    QApplication::setPalette(originalPalette);
}

void formattedTextUsesOwningDisplayDevicePixelRatio() {
    QMimeData mime;
    mime.setHtml(QStringLiteral(
        "<p style=\"font-size: 24px; margin: 0\"><b>Display DPI</b> text</p>"));

    const auto standard = ScreenshotClipboardContentReader::readMimeData(&mime, 1.0);
    const auto highDpi = ScreenshotClipboardContentReader::readMimeData(&mime, 2.0);
    require(standard.has_value() && highDpi.has_value() && standard->isFormattedText() &&
                highDpi->isFormattedText(),
            "formatted text should render at both standard and high display DPRs");
    require(highDpi->image.size() == standard->image.size() * 2 &&
                qFuzzyCompare(highDpi->image.devicePixelRatio(), 2.0) &&
                qFuzzyCompare(highDpi->formattedTextDevicePixelRatio, 2.0),
            "formatted text should allocate its physical backing image from the owning display "
            "DPR");
    require(qFuzzyCompare(highDpi->image.deviceIndependentSize().width(),
                          standard->image.deviceIndependentSize().width()) &&
                qFuzzyCompare(highDpi->image.deviceIndependentSize().height(),
                              standard->image.deviceIndependentSize().height()) &&
                qFuzzyCompare(highDpi->formattedDocument->textWidth(),
                              standard->formattedDocument->textWidth()),
            "display DPR should not change the formatted document's logical layout size");
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
    formattedTextHasPaddingOnEverySide();
    formattedTextBackgroundFollowsApplicationTheme();
    htmlSourceBackgroundOverridesApplicationTheme();
    formattedTextUsesOwningDisplayDevicePixelRatio();
    return 0;
}
