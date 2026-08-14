#include "snow_shot/presentation/screenshothistoryservice.h"
#include "snowimageqtcodec.h"

#include "snow_shot/presentation/screenshotcapturestate.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotintelligentselectionmodel.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotoverlayinputhandler.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"

#include "snow_draw_engine_qt/snow_canvas_runtime.h"
#include "snow_draw_engine_qt/snow_canvas_widget.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ScreenshotHistoryEntry takeSnapshot(std::optional<ScreenshotHistoryEntry> snapshot,
                                    const char* message) {
    if (!snapshot.has_value()) {
        std::cerr << message << '\n';
        std::exit(1);
    }
    return std::move(*snapshot);
}

void waitForNavigation(ScreenshotHistoryService& history, const char* timeoutMessage) {
    QElapsedTimer timer;
    timer.start();
    while (history.navigationInProgress() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    require(!history.navigationInProgress(), timeoutMessage);
}

QImage solidImage(const QSize& size, QRgb color) {
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

QDir historyDirectory(const QString& configurationDirectory) {
    return QDir(QDir(configurationDirectory).filePath(QStringLiteral("capture_history_records")));
}

bool copyDirectoryRecursively(const QString& source, const QString& destination) {
    if (!QDir().mkpath(destination)) {
        return false;
    }
    QDir sourceDirectory(source);
    QDirIterator iterator(source, QDir::AllEntries | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QString relativePath = sourceDirectory.relativeFilePath(sourcePath);
        const QString destinationPath = QDir(destination).filePath(relativePath);
        if (iterator.fileInfo().isDir()) {
            if (!QDir().mkpath(destinationPath)) {
                return false;
            }
        } else if (!QFile::copy(sourcePath, destinationPath)) {
            return false;
        }
    }
    return true;
}

CapturedDisplayModel display(QString stableId, QString name, QRect canvasRect, QImage image) {
    CapturedDisplayModel result;
    result.stableId = std::move(stableId);
    result.name = std::move(name);
    result.physicalRect = canvasRect;
    result.canvasRect = canvasRect;
    result.imageSourceCanvasRect = canvasRect;
    result.logicalRect = canvasRect;
    result.image = std::move(image);
    result.active = true;
    return result;
}

void requireCanvasHistoryPayload(const QByteArray& payload) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    require(error.error == QJsonParseError::NoError && document.isObject(),
            "canvas history payload is not valid JSON");
    const QJsonObject object = document.object();
    require(object.size() == 3 && object.value(QStringLiteral("schemaVersion")).isDouble() &&
                object.value(QStringLiteral("document")).isObject() &&
                object.value(QStringLiteral("history")).isObject(),
            "canvas history payload contains screenshot-local editor state");
}

void navigationMatchesDisplaysAndRestoresLiveEndpoint(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("A"), QStringLiteral("Left"),
                                   QRect(0, 0, 100, 80),
                                   solidImage(QSize(60, 40), qRgba(255, 0, 0, 255))));
    displays.appendDisplay(display(QStringLiteral("B"), QStringLiteral("Right"),
                                   QRect(100, 0, 100, 80),
                                   solidImage(QSize(80, 60), qRgba(0, 255, 0, 255))));

    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(10, 10, 170, 60));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(true);
    ScreenshotIntelligentSelectionModel intelligent;
    require(intelligent.applyCanvasHitPath({QRectF(10, 10, 170, 60)}, QRectF(0, 0, 200, 80), 1.0),
            "failed to initialize intelligent selection");

    int presentationChanges = 0;
    int intelligentSelectionRequests = 0;
    QVector<bool> loadingStates;
    ScreenshotHistoryService history(
        ScreenshotHistoryServiceContext{
            displays,
            runtime,
            selection,
            interaction,
            intelligent,
            [&presentationChanges]() { ++presentationChanges; },
            [&loadingStates](bool loading) { loadingStates.push_back(loading); },
            [&intelligentSelectionRequests]() { ++intelligentSelectionRequests; },
        },
        root);
    auto saved = takeSnapshot(history.snapshotCurrent(true), "failed to create history entry");
    requireCanvasHistoryPayload(saved.canvasHistory);
    history.commit(std::move(saved));

    std::swap(displays.displayAt(0).stableId, displays.displayAt(1).stableId);
    std::swap(displays.displayAt(0).name, displays.displayAt(1).name);
    displays.displayAt(0).image = solidImage(QSize(100, 80), qRgba(0, 0, 255, 255));
    displays.displayAt(1).image = solidImage(QSize(100, 80), qRgba(255, 255, 0, 255));
    const QImage liveFirst = displays.displayAt(0).image;
    const QImage liveSecond = displays.displayAt(1).image;
    selection.setSelectionRect(QRectF(20, 15, 40, 30));

    require(history.navigatePrevious(), "previous history navigation failed");
    require(history.navigationInProgress(),
            "persistent history navigation did not start asynchronously");
    require(displays.displayAt(0).image == liveFirst && displays.displayAt(1).image == liveSecond,
            "asynchronous history navigation changed displays before completion");
    require(!history.navigatePrevious(), "concurrent history navigation was accepted");
    waitForNavigation(history, "previous history navigation timed out");
    require(interaction.manualSelecting(), "persistent entry did not enter manual mode");
    require(displays.displayAt(0).image.pixel(0, 0) == qRgba(0, 255, 0, 255),
            "stable-id monitor matching failed");
    require(displays.displayAt(0).imageSourceCanvasRect == QRect(0, 0, 80, 60),
            "historical image was not placed at native size");
    require(loadingStates == QVector<bool>({true, false}),
            "current-session disk loading did not bracket navigation");
    require(!history.navigatePrevious(), "oldest boundary should be a no-op");
    require(intelligentSelectionRequests == 0,
            "historical entry unexpectedly requested intelligent selection");

    require(history.navigateNext(), "live endpoint navigation failed");
    require(interaction.intelligentSelecting(), "live intelligent mode was not restored");
    require(displays.displayAt(0).image == liveFirst, "first live image was not restored");
    require(displays.displayAt(1).image == liveSecond, "second live image was not restored");
    require(presentationChanges == 2, "unexpected presentation update count");
    require(intelligentSelectionRequests == 1,
            "returning to live did not request intelligent selection exactly once");

    const QRect updatedLiveSelection(30, 20, 50, 40);
    selection.setSelectionRect(updatedLiveSelection);
    interaction.confirmSelection();
    require(history.navigatePrevious(), "second history navigation failed");
    waitForNavigation(history, "second history navigation timed out");
    require(history.navigateNext(), "second live endpoint navigation failed");
    require(interaction.movingSelection(), "updated live selection stage was not recorded");
    require(selection.pixelSelection() == updatedLiveSelection,
            "updated live selection was not recorded");
    require(intelligentSelectionRequests == 1,
            "confirmed live selection unexpectedly requested intelligent selection");
    require(presentationChanges == 4, "second traversal did not update presentation");
    history.drainPendingWrites();
}

void navigationSharesCanvasCreationStyles(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 64, 64),
                                   solidImage(QSize(64, 64), qRgba(20, 30, 40, 255))));
    SnowCanvasRuntime runtime;
    SnowCanvasWidget canvas(runtime);
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 64, 64));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root);

    require(canvas.setCanvasTool(SnowCanvasTool::Shape),
            "failed to activate the historical shape tool");
    SnowCanvasShapeStyle historicalStyle = canvas.canvasStyleToolbarState().shapeStyle;
    historicalStyle.stroke = QColor(17, 34, 51, 255);
    historicalStyle.strokeWidth = 13.0;
    require(canvas.setCanvasShapeStylePatch(historicalStyle,
                                            SnowCanvasShapeStylePropertyStrokeColor |
                                                SnowCanvasShapeStylePropertyStrokeWidth,
                                            SnowCanvasShapeKind::Rectangle),
            "failed to configure the historical creation style");
    auto entry =
        takeSnapshot(history.snapshotCurrent(true), "failed to snapshot the styled history entry");
    requireCanvasHistoryPayload(entry.canvasHistory);
    history.commit(std::move(entry));

    SnowCanvasShapeStyle liveStyle = historicalStyle;
    liveStyle.stroke = QColor(204, 85, 102, 255);
    liveStyle.strokeWidth = 7.0;
    require(canvas.setCanvasShapeStylePatch(liveStyle,
                                            SnowCanvasShapeStylePropertyStrokeColor |
                                                SnowCanvasShapeStylePropertyStrokeWidth,
                                            SnowCanvasShapeKind::Rectangle),
            "failed to configure the live creation style");

    require(history.navigatePrevious(), "styled history navigation failed");
    waitForNavigation(history, "styled history navigation timed out");
    require(canvas.canvasTool() == SnowCanvasTool::Select,
            "history navigation restored a transient canvas tool");
    require(canvas.setCanvasTool(SnowCanvasTool::Shape),
            "failed to reactivate shape after historical restore");
    const SnowCanvasShapeStyle restoredHistorical = canvas.canvasStyleToolbarState().shapeStyle;
    require(restoredHistorical.stroke == liveStyle.stroke &&
                restoredHistorical.strokeWidth == liveStyle.strokeWidth,
            "history navigation did not retain the shared canvas creation style");

    SnowCanvasShapeStyle sharedStyle = restoredHistorical;
    sharedStyle.stroke = QColor(68, 136, 204, 255);
    sharedStyle.strokeWidth = 5.0;
    require(canvas.setCanvasShapeStylePatch(sharedStyle,
                                            SnowCanvasShapeStylePropertyStrokeColor |
                                                SnowCanvasShapeStylePropertyStrokeWidth,
                                            SnowCanvasShapeKind::Rectangle),
            "failed to change the shared creation style from screenshot history");

    require(history.navigateNext(), "styled live navigation failed");
    require(canvas.setCanvasTool(SnowCanvasTool::Shape),
            "failed to reactivate shape after live restore");
    const SnowCanvasShapeStyle restoredLive = canvas.canvasStyleToolbarState().shapeStyle;
    require(restoredLive.stroke == sharedStyle.stroke &&
                restoredLive.strokeWidth == sharedStyle.strokeWidth,
            "style changed in screenshot history was not shared with the live screenshot");
    history.drainPendingWrites();
}

void legacyFullSessionEntriesRemainReadable(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 32, 32),
                                   solidImage(QSize(32, 32), qRgba(12, 34, 56, 255))));
    const QRgb storedPixel = displays.displayAt(0).image.pixel(0, 0);
    SnowCanvasRuntime runtime;
    SnowCanvasWidget canvas(runtime);
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 32, 32));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root);

    require(canvas.setCanvasTool(SnowCanvasTool::Shape),
            "failed to activate shape for the legacy session");
    SnowCanvasShapeStyle legacyStyle = canvas.canvasStyleToolbarState().shapeStyle;
    legacyStyle.strokeWidth = 13.0;
    require(canvas.setCanvasShapeStylePatch(legacyStyle, SnowCanvasShapeStylePropertyStrokeWidth,
                                            SnowCanvasShapeKind::Rectangle),
            "failed to configure the legacy session style");
    auto entry =
        takeSnapshot(history.snapshotCurrent(true), "failed to snapshot the legacy history entry");
    entry.canvasHistory = runtime.serializeDocumentSession();
    require(!entry.canvasHistory.isEmpty(), "failed to create a legacy canvas payload");
    history.commit(std::move(entry));

    SnowCanvasShapeStyle sharedStyle = legacyStyle;
    sharedStyle.strokeWidth = 7.0;
    require(canvas.setCanvasShapeStylePatch(sharedStyle, SnowCanvasShapeStylePropertyStrokeWidth,
                                            SnowCanvasShapeKind::Rectangle),
            "failed to configure the shared style after the legacy snapshot");
    displays.displayAt(0).image = solidImage(QSize(32, 32), qRgba(200, 210, 220, 255));
    require(history.navigatePrevious(), "legacy history navigation failed");
    waitForNavigation(history, "legacy history navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == storedPixel,
            "legacy full-session canvas payload was not restored");
    require(canvas.setCanvasTool(SnowCanvasTool::Shape),
            "failed to reactivate shape after restoring the legacy session");
    require(canvas.canvasStyleToolbarState().shapeStyle.strokeWidth == sharedStyle.strokeWidth,
            "legacy full-session navigation restored its screenshot-local creation style");
    history.drainPendingWrites();
}

void persistenceAndExactRetentionCutoff(const QString& root) {
    QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-08-03T12:00:00.000Z"), Qt::ISODateWithMs);
    QDateTime cutoff = now.addDays(-7);

    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 64, 64),
                                   solidImage(QSize(64, 64), qRgba(20, 30, 40, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 64, 64));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;

    {
        ScreenshotHistoryService writer(
            {displays, runtime, selection, interaction, intelligent, {}}, root,
            [cutoff]() { return cutoff; });
        auto entry = takeSnapshot(writer.snapshotCurrent(true), "failed to snapshot cutoff entry");
        writer.commit(std::move(entry));
        writer.drainPendingWrites();
    }

    QVector<bool> loadingStates;
    ScreenshotHistoryService reader(
        {
            displays,
            runtime,
            selection,
            interaction,
            intelligent,
            {},
            [&loadingStates](bool loading) { loadingStates.push_back(loading); },
        },
        root, [now]() { return now; });
    require(reader.navigatePrevious(), "entry exactly at cutoff was pruned");
    waitForNavigation(reader, "cutoff entry navigation timed out");
    require(loadingStates == QVector<bool>({true, false}),
            "lazy history loading did not bracket the restore");
    reader.resetCaptureNavigation();
}

void corruptLazyEntryDoesNotBlockOlderEntries(const QString& root) {
    QDateTime clock =
        QDateTime::fromString(QStringLiteral("2026-08-03T12:00:00.000Z"), Qt::ISODateWithMs);
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 64, 64),
                                   solidImage(QSize(64, 64), qRgba(255, 0, 0, 255))));
    const QRgb olderPixel = displays.displayAt(0).image.pixel(0, 0);
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 64, 64));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;

    {
        ScreenshotHistoryService writer(
            {displays, runtime, selection, interaction, intelligent, {}}, root,
            [&clock]() { return clock; });
        auto older = takeSnapshot(writer.snapshotCurrent(true), "failed to snapshot older entry");
        writer.commit(std::move(older));
        clock = clock.addSecs(1);
        displays.displayAt(0).image = solidImage(QSize(64, 64), qRgba(0, 255, 0, 255));
        auto newer = takeSnapshot(writer.snapshotCurrent(true), "failed to snapshot newer entry");
        writer.commit(std::move(newer));
        writer.drainPendingWrites();
    }

    displays.displayAt(0).image = solidImage(QSize(64, 64), qRgba(0, 0, 255, 255));
    QVector<bool> loadingStates;
    ScreenshotHistoryService reader(
        {
            displays,
            runtime,
            selection,
            interaction,
            intelligent,
            {},
            [&loadingStates](bool loading) { loadingStates.push_back(loading); },
        },
        root, [&clock]() { return clock; });
    const QFileInfoList directories =
        historyDirectory(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    require(directories.size() == 2, "history entries were not persisted");
    QFile corrupt(QDir(directories.constLast().absoluteFilePath())
                      .filePath(QStringLiteral("canvas_history.json")));
    require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "failed to open session for corruption");
    require(corrupt.write("{") == 1, "failed to corrupt session");
    corrupt.close();

    const QImage liveImage = displays.displayAt(0).image;
    const QByteArray liveSession = runtime.serializeDocumentSession();
    require(reader.navigatePrevious(), "corrupt entry load was not started");
    waitForNavigation(reader, "corrupt entry navigation timed out");
    require(displays.displayAt(0).image == liveImage, "failed navigation changed the live image");
    require(runtime.serializeDocumentSession() == liveSession,
            "failed navigation changed the live canvas session");
    require(loadingStates == QVector<bool>({true, false}),
            "failed history loading left the loading state active");
    require(reader.navigatePrevious(), "corrupt entry blocked an older entry");
    waitForNavigation(reader, "older valid entry navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == olderPixel,
            "older valid entry was not restored");
    require(loadingStates == QVector<bool>({true, false, true, false}),
            "loading state did not cover the valid entry after a failure");
}

void expiredCurrentEntryCanReturnToConfirmedLiveSelection(const QString& root) {
    QDateTime clock =
        QDateTime::fromString(QStringLiteral("2026-08-03T12:00:00.000Z"), Qt::ISODateWithMs);
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 64, 64),
                                   solidImage(QSize(64, 64), qRgba(255, 0, 0, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 64, 64));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root, [&clock]() { return clock; });
    auto entry = takeSnapshot(history.snapshotCurrent(true), "failed to snapshot expiring entry");
    history.commit(std::move(entry));
    history.drainPendingWrites();

    displays.displayAt(0).image = solidImage(QSize(64, 64), qRgba(0, 0, 255, 255));
    const QRect liveSelection(7, 8, 30, 31);
    selection.setSelectionRect(liveSelection);
    const QImage liveImage = displays.displayAt(0).image;
    require(history.navigatePrevious(), "failed to browse expiring entry");
    require(interaction.movingSelection(),
            "manual live selection was not confirmed before history navigation");
    waitForNavigation(history, "expiring entry navigation timed out");
    clock = clock.addDays(8);
    require(history.navigateNext(), "expired entry could not return to live");
    require(displays.displayAt(0).image == liveImage, "live image was not restored after pruning");
    require(interaction.movingSelection(),
            "returning to live did not restore the confirmed selection stage");
    require(selection.pixelSelection() == liveSelection,
            "returning to live did not restore the confirmed selection");
}

void multipleValidEntriesCanBeTraversed(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 64, 64),
                                   solidImage(QSize(64, 64), qRgba(255, 0, 0, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(1, 1, 20, 20));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root);

    const QRgb olderPixel = displays.displayAt(0).image.pixel(0, 0);
    auto older =
        takeSnapshot(history.snapshotCurrent(true), "failed to snapshot older traversal entry");
    const QString olderId = older.id;
    history.commit(std::move(older));

    displays.displayAt(0).image = solidImage(QSize(64, 64), qRgba(0, 255, 0, 255));
    const QRgb newerPixel = displays.displayAt(0).image.pixel(0, 0);
    selection.setSelectionRect(QRectF(2, 2, 30, 30));
    auto newer =
        takeSnapshot(history.snapshotCurrent(true), "failed to snapshot newer traversal entry");
    history.commit(std::move(newer));

    displays.displayAt(0).image = solidImage(QSize(64, 64), qRgba(0, 0, 255, 255));
    const QImage liveImage = displays.displayAt(0).image;
    selection.setSelectionRect(QRectF(3, 3, 40, 40));
    require(!history.navigateToRecord(QStringLiteral("missing-record")) &&
                !history.navigationInProgress() && displays.displayAt(0).image == liveImage,
            "unknown direct history navigation changed the live endpoint");
    require(history.navigateToRecord(olderId), "failed to navigate directly to older entry");
    waitForNavigation(history, "direct older entry navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == olderPixel,
            "direct navigation did not apply the requested older entry");
    require(history.navigateNext(), "failed to navigate from older to newer entry");
    waitForNavigation(history, "newer entry navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == newerPixel,
            "newer traversal entry was not applied after direct navigation");
    require(history.returnToCurrentScreenshot() && displays.displayAt(0).image == liveImage,
            "direct return did not restore the live endpoint");
}

void persistentEntriesAreRereadFromDisk(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 32, 32),
                                   solidImage(QSize(32, 32), qRgba(255, 0, 0, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 32, 32));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root);

    const QRgb storedPixel = displays.displayAt(0).image.pixel(0, 0);
    auto entry =
        takeSnapshot(history.snapshotCurrent(true), "failed to snapshot disk reread entry");
    history.commit(std::move(entry));
    history.drainPendingWrites();

    displays.displayAt(0).image = solidImage(QSize(32, 32), qRgba(0, 0, 255, 255));
    const QImage liveImage = displays.displayAt(0).image;
    require(history.navigatePrevious(), "first disk reread navigation failed");
    waitForNavigation(history, "first disk reread navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == storedPixel,
            "first disk read did not restore the stored image");
    require(history.navigateNext(), "failed to return to live before disk mutation");
    require(displays.displayAt(0).image == liveImage,
            "live endpoint was not restored before disk mutation");

    const QFileInfoList directories =
        historyDirectory(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    require(directories.size() == 1, "disk reread entry directory is missing");
    const QString imagePath =
        QDir(directories.constFirst().absoluteFilePath()).filePath(QStringLiteral("display_0.png"));
    const QImage replacement = solidImage(QSize(32, 32), qRgba(0, 255, 0, 255));
    const QByteArray replacementPng = snow_shot::image_codec::encodePng(replacement);
    QFile replacementFile(imagePath);
    require(!replacementPng.isEmpty() && replacementFile.open(QIODevice::WriteOnly) &&
                replacementFile.write(replacementPng) == replacementPng.size(),
            "failed to replace the persisted history image");

    require(history.navigatePrevious(), "second disk reread navigation failed");
    waitForNavigation(history, "second disk reread navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == qRgba(0, 255, 0, 255),
            "history navigation reused a cached image instead of rereading disk");
}

void publicationCreatesSelfContainedRecord(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 32, 32),
                                   solidImage(QSize(32, 32), qRgba(10, 20, 30, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 32, 32));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     root);
    auto entry = takeSnapshot(history.snapshotCurrent(true), "failed to snapshot direct entry");
    history.commit(std::move(entry));

    QElapsedTimer timer;
    timer.start();
    QFileInfoList directories;
    while (directories.isEmpty() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
        directories =
            historyDirectory(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    }
    require(directories.size() == 1 &&
                QFileInfo(QDir(directories.constFirst().absoluteFilePath())
                              .filePath(QStringLiteral("canvas_history.json")))
                    .isFile() &&
                QFileInfo(QDir(directories.constFirst().absoluteFilePath())
                              .filePath(QStringLiteral("display_0.png")))
                    .isFile() &&
                QFileInfo(QDir(directories.constFirst().absoluteFilePath())
                              .filePath(QStringLiteral("manifest.json")))
                    .isFile(),
            "history record was not published as a self-contained directory");
    require(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("config.json"))),
            "history publication unexpectedly wrote configuration state");
    history.drainPendingWrites();
    require(
        !QFileInfo::exists(QDir(root).filePath(QStringLiteral("capture_history_catalog.json"))) &&
            !QFileInfo::exists(QDir(root).filePath(QStringLiteral("config.json"))),
        "history drain unexpectedly created shared storage metadata");
}

void startupReconciliationQuarantinesBrokenAndOrphanedEntries(const QString& root) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 32, 32),
                                   solidImage(QSize(32, 32), qRgba(10, 20, 30, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 32, 32));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    {
        ScreenshotHistoryService writer(
            {displays, runtime, selection, interaction, intelligent, {}}, root);
        auto entry = takeSnapshot(writer.snapshotCurrent(true), "failed to snapshot cleanup entry");
        writer.commit(std::move(entry));
        writer.drainPendingWrites();
    }

    QDir managed = historyDirectory(root);
    const QFileInfoList records =
        managed.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    require(records.size() == 1, "cleanup entry was not published");
    require(QFile::remove(QDir(records.constFirst().absoluteFilePath())
                              .filePath(QStringLiteral("display_0.png"))),
            "failed to remove indexed display image");
    require(managed.mkdir(QStringLiteral(".tmp-interrupted")) &&
                managed.mkdir(QStringLiteral("orphan-entry")),
            "failed to create reconciliation fixtures");

    {
        ScreenshotHistoryService reader(
            {displays, runtime, selection, interaction, intelligent, {}}, root);
        require(!reader.navigatePrevious(), "startup reconciliation retained a broken record");
        reader.drainPendingWrites();
    }
    require(managed.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty(),
            "startup reconciliation retained invalid record directories");
    require(!QFileInfo::exists(managed.filePath(QStringLiteral(".tmp-interrupted"))),
            "startup reconciliation retained an interrupted temporary directory");
    const QDir quarantine(QDir(root).filePath(QStringLiteral("capture_history_quarantine")));
    require(quarantine.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot).size() == 2,
            "startup reconciliation did not preserve invalid records in quarantine");
    require(!QFileInfo::exists(QDir(root).filePath(QStringLiteral("capture_history_catalog.json"))),
            "startup reconciliation unexpectedly created a catalog");
}

void portableDirectoryCopyKeepsHistoryValid(const QString& sourceRoot, const QString& copiedRoot) {
    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 24, 24),
                                   solidImage(QSize(24, 24), qRgba(90, 80, 70, 255))));
    const QRgb savedPixel = displays.displayAt(0).image.pixel(0, 0);
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 24, 24));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    {
        ScreenshotHistoryService writer(
            {displays, runtime, selection, interaction, intelligent, {}}, sourceRoot);
        auto entry =
            takeSnapshot(writer.snapshotCurrent(true), "failed to snapshot portable entry");
        writer.commit(std::move(entry));
        writer.drainPendingWrites();
    }
    require(copyDirectoryRecursively(sourceRoot, copiedRoot),
            "failed to copy portable configuration directory");

    displays.displayAt(0).image = solidImage(QSize(24, 24), qRgba(1, 2, 3, 255));
    ScreenshotHistoryService reader({displays, runtime, selection, interaction, intelligent, {}},
                                    copiedRoot);
    require(reader.navigatePrevious(), "copied portable history was not indexed");
    waitForNavigation(reader, "copied portable history navigation timed out");
    require(displays.displayAt(0).image.pixel(0, 0) == savedPixel,
            "relative history paths did not survive a portable directory copy");
}

void failedPublicationDoesNotInsertRecord(const QString& rootFile) {
    QFile blockingFile(rootFile);
    require(blockingFile.open(QIODevice::WriteOnly), "failed to create blocked history root");
    require(blockingFile.write("blocked") == 7, "failed to write blocked history root");
    blockingFile.close();

    ScreenshotDisplaySession displays;
    displays.appendDisplay(display(QStringLiteral("only"), QStringLiteral("Only"),
                                   QRect(0, 0, 16, 16),
                                   solidImage(QSize(16, 16), qRgba(4, 5, 6, 255))));
    SnowCanvasRuntime runtime;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(0, 0, 16, 16));
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(false);
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotHistoryService history({displays, runtime, selection, interaction, intelligent, {}},
                                     rootFile);
    auto entry = takeSnapshot(history.snapshotCurrent(true), "failed to snapshot rejected entry");
    history.commit(std::move(entry));
    history.drainPendingWrites();
    require(!history.navigatePrevious(), "failed publication inserted a navigable record");
}

void historyKeysOnlyWorkDuringSelectionStates() {
    ScreenshotCaptureState captureState;
    ScreenshotDisplaySession displays;
    ScreenshotGeometryMapper geometry;
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(true);
    intelligent.beginPress(QPointF(4, 4), QRectF(0, 0, 8, 8));

    int previousCount = 0;
    int nextCount = 0;
    int pauseCount = 0;
    int showToolbarCount = 0;
    int selectionConfirmedCount = 0;
    int returnToCurrentCount = 0;
    int returnToIntelligentCount = 0;
    bool hasCurrentScreenshot = false;
    ScreenshotOverlayInputActions actions;
    actions.navigateHistoryPrevious = [&previousCount]() {
        ++previousCount;
        return true;
    };
    actions.navigateHistoryNext = [&nextCount]() {
        ++nextCount;
        return true;
    };
    actions.returnToCurrentScreenshot = [&returnToCurrentCount, &hasCurrentScreenshot]() {
        ++returnToCurrentCount;
        return hasCurrentScreenshot;
    };
    actions.returnToIntelligentSelection = [&returnToIntelligentCount](const QPoint&) {
        ++returnToIntelligentCount;
        return true;
    };
    actions.pauseIntelligentSelection = [&pauseCount]() { ++pauseCount; };
    actions.showToolbar = [&showToolbarCount]() { ++showToolbarCount; };
    actions.selectionConfirmed = [&selectionConfirmedCount]() { ++selectionConfirmedCount; };
    ScreenshotOverlayInputHandler handler({
        captureState,
        interaction,
        selection,
        intelligent,
        geometry,
        displays,
        std::move(actions),
    });

    handler.confirmSelection();
    require(selectionConfirmedCount == 0 &&
                captureState.sessionState != ScreenshotSessionState::Editing &&
                !interaction.movingSelection(),
            "an empty selection must not trigger post-selection actions");

    require(handler.handleKeyPress(Qt::Key_Comma, {}),
            "comma key was not handled during smart selection");
    require(previousCount == 1, "comma key did not navigate history during smart selection");
    require(!intelligent.pressActive(),
            "history navigation did not cancel the pending smart selection press");
    require(pauseCount == 1,
            "history navigation did not cancel the pending smart selection request");

    interaction.enterOverlayVisible(false);
    selection.setSelectionRect(QRectF(1, 2, 20, 21));
    require(handler.handleKeyPress(Qt::Key_Period, {}),
            "period key was not handled during manual selection");
    require(nextCount == 1, "period key did not navigate history during manual selection");
    require(interaction.movingSelection(),
            "manual selection was not confirmed before history navigation");
    require(captureState.sessionState == ScreenshotSessionState::Editing,
            "confirming manual selection did not enter the editing session state");
    require(showToolbarCount == 1, "confirming manual selection did not show the toolbar");
    require(selectionConfirmedCount == 1,
            "confirming manual selection did not notify post-selection actions");

    require(handler.handleRightClick(nullptr, QPointF(4, 4)),
            "right-click did not handle manual selection");
    require(returnToCurrentCount == 1,
            "right-click did not check for an active historical screenshot");
    require(returnToIntelligentCount == 1,
            "ordinary right-click did not return to intelligent selection");

    hasCurrentScreenshot = true;
    require(handler.handleRightClick(nullptr, QPointF(4, 4)),
            "right-click did not handle historical selection");
    require(returnToCurrentCount == 2,
            "historical right-click did not return to the current screenshot");
    require(returnToIntelligentCount == 1,
            "historical right-click incorrectly returned to intelligent selection");

    interaction.enterManualSelectionDrag();
    require(handler.handleKeyPress(Qt::Key_Comma, {}),
            "comma key was not handled during manual box selection");
    require(previousCount == 2, "comma key did not navigate history during manual box selection");
    require(!interaction.dragging(),
            "history navigation did not cancel the active manual selection drag");
    require(interaction.movingSelection(),
            "manual selection drag was not finalized before history navigation");

    interaction.confirmSelection();
    require(handler.handleKeyPress(Qt::Key_Period, {}),
            "period key was not handled after confirming the selection");
    require(nextCount == 2, "period key did not navigate history after selection confirmation");
    require(pauseCount == 4,
            "history navigation did not consistently cancel smart selection requests");

    interaction.setCanvasTool(ScreenshotActiveTool::Shape);
    require(!handler.handleKeyPress(Qt::Key_Comma, {}), "comma key was handled while editing");
    require(previousCount == 2, "comma key navigated history while editing");
}

void moveToolCanStartManualSelectionOutsideConfirmedBox() {
    ScreenshotCaptureState captureState;
    ScreenshotDisplaySession displays;
    ScreenshotGeometryMapper geometry;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(10, 10, 20, 20));
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotInteractionState interaction;
    interaction.confirmSelection();

    int overlayUpdates = 0;
    int guideLineUpdates = 0;
    ScreenshotOverlayInputActions actions;
    actions.updateOverlayState = [&overlayUpdates]() { ++overlayUpdates; };
    actions.updateGuideLinesForOverlay = [&guideLineUpdates](ScreenshotOverlayWindow*,
                                                              const QPointF&) {
        ++guideLineUpdates;
    };
    ScreenshotOverlayInputHandler handler({
        captureState,
        interaction,
        selection,
        intelligent,
        geometry,
        displays,
        std::move(actions),
    });

    require(handler.shouldHandleMouseEvent(nullptr, QPointF(50, 50), true),
            "Move must handle a press outside the confirmed selection");
    handler.handleMousePress(nullptr, QPointF(50, 50));
    require(interaction.manualSelecting() && interaction.dragging(),
            "Move press outside the selection must enter manual drag mode");
    require(selection.normalizedSelection() == QRectF(50, 50, 0, 0),
            "manual drag did not reset the selection origin");
    require(overlayUpdates == 1, "manual drag did not refresh the overlay");
    require(guideLineUpdates == 1, "manual drag did not update the cursor guide lines");
}

void completionGesturesRequireAConfirmedSelectionAndSupportedTool() {
    ScreenshotCaptureState captureState;
    ScreenshotDisplaySession displays;
    ScreenshotGeometryMapper geometry;
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotInteractionState interaction;
    interaction.enterOverlayVisible(true);

    int actionCount = 0;
    ScreenshotOverlayInputActions actions;
    actions.executeConfiguredCompletionAction = [&actionCount](const QString&) { ++actionCount; };
    ScreenshotOverlayInputHandler handler({
        captureState,
        interaction,
        selection,
        intelligent,
        geometry,
        displays,
        std::move(actions),
    });

    handler.handleUnhandledLeftDoubleClick();
    handler.handleUnhandledMiddleClick();
    require(actionCount == 0,
            "completion gestures must not run while the initial selection is active");

    selection.setSelectionRect(QRectF(1, 2, 20, 21));
    interaction.confirmSelection();
    handler.handleUnhandledLeftDoubleClick();
    require(actionCount == 1, "double-click must run for the confirmed Move tool");

    interaction.setCanvasTool(ScreenshotActiveTool::Shape);
    handler.handleUnhandledMiddleClick();
    require(actionCount == 2, "middle-click must run for a drawing tool");

    interaction.setCanvasTool(ScreenshotActiveTool::Select);
    handler.handleUnhandledLeftDoubleClick();
    interaction.enterScrollingCapture();
    handler.handleUnhandledMiddleClick();
    require(actionCount == 2,
            "completion gestures must ignore Select and scrolling screenshot modes");
}

void drawingShortcutsTakePriorityOverMoveColorPickerKeys() {
    ScreenshotCaptureState captureState;
    ScreenshotDisplaySession displays;
    ScreenshotGeometryMapper geometry;
    ScreenshotSelectionModel selection;
    selection.setSelectionRect(QRectF(1, 2, 20, 21));
    ScreenshotIntelligentSelectionModel intelligent;
    ScreenshotInteractionState interaction;
    interaction.confirmSelection();

    QString activatedTool;
    int colorPickerMoves = 0;
    ScreenshotOverlayInputActions actions;
    actions.activateDrawingShortcut = [&activatedTool](const QString& toolId) {
        activatedTool = toolId;
        return true;
    };
    actions.moveColorPickerCursor = [&colorPickerMoves](int, int) {
        ++colorPickerMoves;
        return true;
    };
    ScreenshotOverlayInputHandler handler({
        captureState,
        interaction,
        selection,
        intelligent,
        geometry,
        displays,
        std::move(actions),
    });

    require(handler.handleKeyPress(Qt::Key_S, {}), "default Shape shortcut was not handled");
    require(activatedTool == QStringLiteral("shape") && colorPickerMoves == 0,
            "default Shape shortcut must win over Move color-picker navigation");

    activatedTool.clear();
    require(handler.handleKeyPress(Qt::Key_Up, {}),
            "non-conflicting color-picker navigation was not handled");
    require(activatedTool.isEmpty() && colorPickerMoves == 1,
            "arrow-key color-picker navigation should remain available");
}
} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory unavailable");
    navigationMatchesDisplaysAndRestoresLiveEndpoint(
        QDir(temporary.path()).filePath(QStringLiteral("navigation")));
    navigationSharesCanvasCreationStyles(
        QDir(temporary.path()).filePath(QStringLiteral("creation-styles")));
    legacyFullSessionEntriesRemainReadable(
        QDir(temporary.path()).filePath(QStringLiteral("legacy-payload")));
    persistenceAndExactRetentionCutoff(
        QDir(temporary.path()).filePath(QStringLiteral("retention")));
    corruptLazyEntryDoesNotBlockOlderEntries(
        QDir(temporary.path()).filePath(QStringLiteral("corrupt")));
    expiredCurrentEntryCanReturnToConfirmedLiveSelection(
        QDir(temporary.path()).filePath(QStringLiteral("expired-navigation")));
    multipleValidEntriesCanBeTraversed(
        QDir(temporary.path()).filePath(QStringLiteral("multi-entry-navigation")));
    persistentEntriesAreRereadFromDisk(
        QDir(temporary.path()).filePath(QStringLiteral("disk-reread")));
    publicationCreatesSelfContainedRecord(
        QDir(temporary.path()).filePath(QStringLiteral("direct-publication")));
    startupReconciliationQuarantinesBrokenAndOrphanedEntries(
        QDir(temporary.path()).filePath(QStringLiteral("startup-reconciliation")));
    portableDirectoryCopyKeepsHistoryValid(
        QDir(temporary.path()).filePath(QStringLiteral("portable-source")),
        QDir(temporary.path()).filePath(QStringLiteral("portable-copy")));
    failedPublicationDoesNotInsertRecord(
        QDir(temporary.path()).filePath(QStringLiteral("blocked-history-root")));
    historyKeysOnlyWorkDuringSelectionStates();
    moveToolCanStartManualSelectionOutsideConfirmedBox();
    completionGesturesRequireAConfirmedSelectionAndSupportedTool();
    drawingShortcutsTakePriorityOverMoveColorPickerKeys();
    return 0;
}
