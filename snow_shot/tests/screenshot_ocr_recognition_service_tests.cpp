#include "snow_shot/presentation/screenshotocrrecognitionservice.h"

#include "snow_ocr_c.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QTimer>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

constexpr int kSkipped = 77;
constexpr int kRecognitionTimeoutMs = 30'000;

QImage whiteImage(int edge = 64) {
    QImage image(edge, edge, QImage::Format_RGBA8888);
    image.fill(Qt::white);
    return image;
}

SnowOcrResourceCountsV1 resourceCounts() {
    SnowOcrResourceCountsV1 counts{};
    counts.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrResourceCountsV1));
    require(snow_ocr_resource_counts_v1(&counts) != 0,
            "OCR resource counts should be available");
    return counts;
}

int expectedWorkerCount(int requestCount) {
    SnowOcrRuntimeInfoV1 info{};
    info.struct_size = static_cast<std::uint32_t>(sizeof(SnowOcrRuntimeInfoV1));
    require(snow_ocr_runtime_info_v1(&info) != 0, "OCR runtime info should be available");
    const int budget = (std::max)(1, static_cast<int>(info.physical_core_count / 2));
    return (std::min)(requestCount, budget);
}

bool waitUntil(const std::function<bool()>& condition, int timeoutMs) {
    if (condition()) {
        return true;
    }
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;
    poll.setInterval(5);
    timeout.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (condition()) {
            loop.quit();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    timeout.start(timeoutMs);
    loop.exec();
    return condition();
}

void processEventsFor(int durationMs) {
    QEventLoop loop;
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec();
}

void embeddedEngineCompletesThroughTheQtWorker(bool directMlEnabled) {
    ScreenshotOcrRecognitionService service([directMlEnabled]() { return directMlEnabled; });
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

    const QImage image = whiteImage();
    const ScreenshotOcrRecognitionPort::RequestToken token =
        service.recognize(image, QRectF(QPointF(), QSizeF(image.size())), &loop,
                          [&](ScreenshotOcrRecognitionResult result) {
                              output = std::move(result);
                              completed = true;
                              loop.quit();
                          });

    require(token != 0, "a valid OCR image should schedule recognition");
    timeout.start(kRecognitionTimeoutMs);
    loop.exec();

    require(!timedOut && completed, "OCR recognition should complete within the test timeout");
    require(output.error.isEmpty(), "the embedded OCR engine should not report an error");
    require(output.presentation != nullptr, "OCR recognition should return a presentation");
}

void concurrentRequestsCompleteExactlyOnce() {
    constexpr int kRequestCount = 3;
    qputenv("SNOW_SHOT_OCR_IDLE_TIMEOUT_MS", "30000");
    ScreenshotOcrRecognitionService service;
    QObject receiver;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    int completions = 0;
    std::vector<ScreenshotOcrRecognitionResult> outputs;
    outputs.reserve(kRequestCount);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });

    for (int index = 0; index < kRequestCount; ++index) {
        const QImage image = whiteImage();
        const auto token = service.recognize(
            image, QRectF(QPointF(index, index), QSizeF(image.size())), &receiver,
            [&](ScreenshotOcrRecognitionResult result) {
                ++completions;
                outputs.push_back(std::move(result));
                if (completions == kRequestCount) {
                    loop.quit();
                }
            });
        require(token != 0, "every concurrent OCR request should be accepted");
    }

    timeout.start(kRecognitionTimeoutMs);
    loop.exec();
    require(!timedOut, "concurrent OCR requests should finish within the test timeout");
    require(completions == kRequestCount, "every concurrent OCR request should complete once");
    for (const auto& output : outputs) {
        require(output.error.isEmpty(), "concurrent OCR should not report an error");
        require(output.presentation != nullptr,
                "every concurrent OCR request should return a presentation");
    }

    SnowOcrResourceCountsV1 counts = resourceCounts();
    require(counts.engines == static_cast<std::size_t>(expectedWorkerCount(kRequestCount)),
            "concurrent OCR should create one exclusive engine per budgeted worker");
    require(counts.results == 0, "FFI results should be released before Qt delivery");
    require(counts.owned_images == kRequestCount,
            "each live presentation should own exactly one transferred image");

    outputs.clear();
    counts = resourceCounts();
    require(counts.owned_images == 0,
            "releasing presentations should release every transferred image");
}

void idleWorkersRetireToZero() {
    qputenv("SNOW_SHOT_OCR_IDLE_TIMEOUT_MS", "150");
    require(resourceCounts().engines == 0,
            "the previous OCR service should destroy its engines during shutdown");
    ScreenshotOcrRecognitionService service;
    QObject receiver;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    ScreenshotOcrRecognitionResult output;
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });
    const QImage image = whiteImage();
    const auto token = service.recognize(
        image, QRectF(QPointF(), QSizeF(image.size())), &receiver,
        [&](ScreenshotOcrRecognitionResult result) {
            output = std::move(result);
            loop.quit();
        });
    require(token != 0, "the idle-retirement OCR request should be accepted");
    timeout.start(kRecognitionTimeoutMs);
    loop.exec();
    require(!timedOut && output.presentation != nullptr && output.error.isEmpty(),
            "the idle-retirement OCR request should complete successfully");
    require(resourceCounts().engines == 1,
            "a completed OCR worker should retain its engine until the idle timeout");
    require(resourceCounts().owned_images == 1,
            "the delivered OCR presentation should own its transferred image");

    output.presentation.reset();
    require(resourceCounts().owned_images == 0,
            "releasing the presentation should release its transferred image");
    require(waitUntil([]() { return resourceCounts().engines == 0; }, 10'000),
            "idle workers should destroy every OCR engine");
    const SnowOcrResourceCountsV1 counts = resourceCounts();
    require(counts.results == 0 && counts.owned_images == 0,
            "idle retirement should leave no live OCR result resources");
}

void cancellationSuppressesCompletion() {
    ScreenshotOcrRecognitionService service;
    QObject receiver;
    bool completed = false;
    const QImage image = whiteImage();
    const auto token = service.recognize(
        image, QRectF(QPointF(), QSizeF(image.size())), &receiver,
        [&](ScreenshotOcrRecognitionResult) { completed = true; });
    require(token != 0, "the cancellable OCR request should be accepted");
    service.cancel(token);
    processEventsFor(250);
    require(!completed, "an immediately cancelled OCR request must not invoke its completion");
}

void receiverDestructionSuppressesCompletion() {
    ScreenshotOcrRecognitionService service;
    auto receiver = std::make_unique<QObject>();
    bool completed = false;
    const QImage image = whiteImage();
    const auto token = service.recognize(
        image, QRectF(QPointF(), QSizeF(image.size())), receiver.get(),
        [&](ScreenshotOcrRecognitionResult) { completed = true; });
    require(token != 0, "the receiver-guarded OCR request should be accepted");
    receiver.reset();
    processEventsFor(250);
    require(!completed, "destroying the receiver must suppress OCR completion");
}

void serviceDestructionJoinsWorkersAndSuppressesLateDelivery() {
    QObject receiver;
    int completions = 0;
    auto service = std::make_unique<ScreenshotOcrRecognitionService>();
    for (int index = 0; index < 3; ++index) {
        const QImage image = whiteImage(256);
        const auto token = service->recognize(
            image, QRectF(QPointF(), QSizeF(image.size())), &receiver,
            [&](ScreenshotOcrRecognitionResult) { ++completions; });
        require(token != 0, "requests queued before service shutdown should be accepted");
    }

    require(waitUntil([]() { return resourceCounts().engines > 0; }, kRecognitionTimeoutMs),
            "at least one OCR worker should initialize before shutdown");
    const int completionsBeforeDestruction = completions;
    service.reset();
    processEventsFor(250);
    require(completions == completionsBeforeDestruction,
            "destroyed OCR services must not deliver queued completions");
    const SnowOcrResourceCountsV1 counts = resourceCounts();
    require(counts.engines == 0 && counts.results == 0 && counts.owned_images == 0,
            "service destruction should synchronously join workers and release FFI resources");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    const bool directMlRequested = application.arguments().contains(QStringLiteral("--directml"));
    if (directMlRequested && snow_ocr_directml_is_available() == 0) {
        std::cout << "DirectML is unavailable in the current test environment\n";
        return kSkipped;
    }
    embeddedEngineCompletesThroughTheQtWorker(directMlRequested);
    if (!directMlRequested) {
        concurrentRequestsCompleteExactlyOnce();
        idleWorkersRetireToZero();
        cancellationSuppressesCompletion();
        receiverDestructionSuppressesCompletion();
        serviceDestructionJoinsWorkersAndSuppressesLateDelivery();
    }
    return 0;
}
