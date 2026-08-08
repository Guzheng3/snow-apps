#include "snow_shot/presentation/screenshotocrrecognitionservice.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QTimer>

#include <cstdlib>
#include <iostream>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void embeddedEngineCompletesThroughTheQtWorker() {
    ScreenshotOcrRecognitionService service;
    QEventLoop loop;
    ScreenshotOcrRecognitionResult output;
    bool completed = false;
    bool timedOut = false;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });

    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    const ScreenshotOcrRecognitionPort::RequestToken token =
        service.recognize(image, QRectF(QPointF(), QSizeF(image.size())), &loop,
                          [&](ScreenshotOcrRecognitionResult result) {
                              output = std::move(result);
                              completed = true;
                              loop.quit();
                          });

    require(token != 0, "a valid OCR image should schedule recognition");
    timeout.start(10000);
    loop.exec();

    require(!timedOut && completed, "OCR recognition should complete within the test timeout");
    require(output.error.isEmpty(), "the embedded OCR engine should not report an error");
    require(output.presentation != nullptr, "OCR recognition should return a presentation");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    embeddedEngineCompletesThroughTheQtWorker();
    return 0;
}
