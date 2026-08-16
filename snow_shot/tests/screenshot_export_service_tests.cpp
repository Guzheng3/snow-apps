#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotdefaultstyles.h"
#include "snow_shot/presentation/screenshotexportservice.h"
#include "snow_shot/presentation/screenshotresultcompositor.h"

#include "snow_draw_engine_qt/snow_canvas_runtime.h"

#include <QApplication>
#include <QEventLoop>
#include <QImage>
#include <QObject>
#include <QTimer>

#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

QImage patternedImage(const QSize& size, int seed) {
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            row[x] = qRgba((x * 17 + seed) % 256, (y * 29 + seed * 3) % 256,
                           (x * 7 + y * 11 + seed * 5) % 256, 255);
        }
    }
    return image;
}

bool hasSamePixels(const QImage& actual, const QImage& expected) {
    return actual.size() == expected.size() && actual.convertToFormat(QImage::Format_ARGB32) ==
                                                   expected.convertToFormat(QImage::Format_ARGB32);
}

class ExportFixture final {
  public:
    ExportFixture()
        : m_runtime(
              SnowCanvasRuntimeConfig{snow_shot::presentation::screenshotCanvasStyleDefaults()}) {
        CapturedDisplayModel display;
        display.stableId = QStringLiteral("display-history-source");
        display.name = QStringLiteral("Display history source");
        display.physicalRect = QRect(0, 0, 80, 60);
        display.canvasRect = display.physicalRect;
        display.imageSourceCanvasRect = display.canvasRect;
        display.logicalRect = display.physicalRect;
        display.image = patternedImage(display.physicalRect.size(), 3);
        display.active = true;
        m_displays.appendDisplay(std::move(display));

        m_service = std::make_unique<ScreenshotExportService>(ScreenshotExportServiceContext{
            m_displays,
            m_runtime,
            m_geometry,
        });
    }

    [[nodiscard]] bool isValid() const {
        return m_runtime.isValid() && m_service != nullptr;
    }

    ScreenshotExportService& service() {
        return *m_service;
    }

    [[nodiscard]] QImage displaySnapshot() const {
        return m_displays.displayAt(0).image;
    }

  private:
    ScreenshotDisplaySession m_displays;
    SnowCanvasRuntime m_runtime;
    ScreenshotGeometryMapper m_geometry;
    std::unique_ptr<ScreenshotExportService> m_service;
};

template <typename ScheduleRequest, typename ResultImage>
QImage waitForResult(ScheduleRequest scheduleRequest, ResultImage resultImage) {
    QObject receiver;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QImage image;
    bool timedOut = false;
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });

    const bool scheduled = scheduleRequest(&receiver, [&](auto result) {
        image = resultImage(std::move(result));
        loop.quit();
    });
    require(scheduled, "selection export was not scheduled");
    timeout.start();
    loop.exec();
    timeout.stop();
    require(!timedOut, "selection export timed out");
    return image;
}

void directSourceKeepsFullWgcFrameForResultAndClipboard() {
    ExportFixture fixture;
    require(fixture.isValid(), "export fixture could not initialize the canvas runtime");

    const QRect visibleSelection(12, 8, 37, 29);
    const ScreenshotResultStyle style;
    const QImage directSource = patternedImage(QSize(137, 91), 11);
    const QImage expected = ScreenshotResultCompositor::compose(directSource, style);
    const QImage displayBefore = fixture.displaySnapshot().copy();

    fixture.service().setNextSelectionSourceImage(directSource);
    const QImage result = waitForResult(
        [&](QObject* receiver, auto callback) {
            return fixture.service().requestSelectionResult(visibleSelection, style, receiver,
                                                            std::move(callback));
        },
        [](QImage image) { return image; });

    require(result.size() == directSource.size(),
            "result export cropped the direct WGC source to the display selection");
    require(hasSamePixels(result, expected), "result export changed the direct WGC source pixels");
    require(fixture.displaySnapshot() == displayBefore,
            "result export modified the display snapshot retained for history");

    fixture.service().setNextSelectionSourceImage(directSource);
    const QImage clipboardResult = waitForResult(
        [&](QObject* receiver, auto callback) {
            return fixture.service().requestSelectionClipboard(visibleSelection, style, receiver,
                                                               std::move(callback));
        },
        [](ScreenshotSelectionClipboardResult result) {
            require(result.isValid(), "clipboard export did not produce a valid payload");
            return std::move(result.image);
        });

    require(clipboardResult.size() == directSource.size(),
            "clipboard export cropped the direct WGC source to the display selection");
    require(hasSamePixels(clipboardResult, expected),
            "clipboard export changed the direct WGC source pixels");
    require(fixture.displaySnapshot() == displayBefore,
            "clipboard export modified the display snapshot retained for history");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    directSourceKeepsFullWgcFrameForResultAndClipboard();
    std::cout << "All screenshot export service tests passed\n";
    return EXIT_SUCCESS;
}
