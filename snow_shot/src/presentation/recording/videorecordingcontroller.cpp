#include "snow_shot/presentation/videorecordingcontroller.h"

#include "snow_shot/presentation/screenshottoolpalette.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/videorecordingareawindow.h"
#include "snow_shot/presentation/videorecordingtoolbarwindow.h"
#include "videorecordinggeometry.h"
#include "snow_shot/storage/settingsadapters.h"

#include "snow_capture.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <chrono>
#include <future>
#include <utility>

namespace {
constexpr int kRecordingFps = 30;
constexpr int kDurationTickMilliseconds = 100;

QString recordingDirectory() {
    QString root = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    return QDir(root).filePath(QStringLiteral("SnowShot"));
}

QString recordingFilePath(bool gif) {
    const QString extension = gif ? QStringLiteral("gif") : QStringLiteral("mp4");
    const QString fileName =
        QStringLiteral("SnowShot_Video_%1.%2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")))
            .arg(extension);
    return QDir(recordingDirectory()).filePath(fileName);
}

QString recordingWorkingDirectory() {
    QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    return QDir(root).filePath(QStringLiteral("recordings"));
}

QString captureError() {
    const char* error = snow_capture_last_error_message();
    return QString::fromUtf8(error != nullptr ? error : "Unknown recording error");
}

void copyFileToClipboard(const QString& filePath) {
    auto* mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(filePath)});
    QApplication::clipboard()->setMimeData(mimeData);
}
} // namespace

struct VideoRecordingController::Impl {
    explicit Impl(VideoRecordingController& owner) : owner(owner) {
        const snow_shot::storage::RecordingSettings settings;
        microphoneEnabled = settings.microphoneEnabled();
        systemAudioEnabled = settings.systemAudioEnabled();
        durationTimer.setInterval(kDurationTickMilliseconds);
        durationTimer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&durationTimer, &QTimer::timeout, &owner, [this]() {
            if (state != ScreenshotToolPalette::RecordingState::Recording) {
                return;
            }
            durationMilliseconds += kDurationTickMilliseconds;
            syncUi();
        });
        exportPollTimer.setInterval(50);
        QObject::connect(&exportPollTimer, &QTimer::timeout, &owner, [this]() { pollExport(); });
    }

    ~Impl() {
        durationTimer.stop();
        exportPollTimer.stop();
        if (exportFuture.valid()) {
            exportFuture.wait();
            exportFuture.get();
        }
        if (recordingSession != nullptr) {
            snow_capture_recording_session_destroy(recordingSession);
        }
        recordingSession = nullptr;
        if (toolbarWindow != nullptr) {
            toolbarWindow->hide();
            toolbarWindow->deleteLater();
        }
        if (areaWindow != nullptr) {
            areaWindow->hide();
            areaWindow->deleteLater();
        }
    }

    void open(const QRect& region) {
        if (!region.isValid() || region.isEmpty() || busy) {
            return;
        }
        if (isOpen()) {
            if (state != ScreenshotToolPalette::RecordingState::Idle) {
                return;
            }
            physicalRegion = region;
            updateCaptureRegion();
            areaWindow->setPhysicalRegion(region);
            toolbarWindow->placeForPhysicalRegion(region);
            areaWindow->show();
            toolbarWindow->show();
            areaWindow->raise();
            toolbarWindow->raise();
            return;
        }

        physicalRegion = region;
        updateCaptureRegion();
        areaWindow = new VideoRecordingAreaWindow();
        toolbarWindow = new VideoRecordingToolbarWindow();
        areaWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        toolbarWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        areaWindow->setPhysicalRegion(region);
        toolbarWindow->placeForPhysicalRegion(region);
        connectToolbar();

        state = ScreenshotToolPalette::RecordingState::Idle;
        durationMilliseconds = 0;
        syncUi();

        areaWindow->show();
        toolbarWindow->show();
        areaWindow->raise();
        toolbarWindow->raise();
    }

    bool isOpen() const {
        return areaWindow != nullptr && toolbarWindow != nullptr &&
               (areaWindow->isVisible() || toolbarWindow->isVisible());
    }

    void connectToolbar() {
        ScreenshotToolPalette* palette =
            toolbarWindow != nullptr ? toolbarWindow->palette() : nullptr;
        if (palette == nullptr) {
            return;
        }
        QObject::connect(palette, &ScreenshotToolPalette::recordingStartRequested, &owner,
                         [this]() { start(); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingStopRequested, &owner,
                         [this]() { stop(false, false, false); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingPauseRequested, &owner,
                         [this]() { pause(); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingResumeRequested, &owner,
                         [this]() { resume(); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingMicrophoneToggled, &owner,
                         [this](bool enabled) {
                             microphoneEnabled = enabled;
                             snow_shot::storage::RecordingSettings().setMicrophoneEnabled(enabled);
                         });
        QObject::connect(palette, &ScreenshotToolPalette::recordingSystemAudioToggled, &owner,
                         [this](bool enabled) {
                             systemAudioEnabled = enabled;
                             snow_shot::storage::RecordingSettings().setSystemAudioEnabled(enabled);
                         });
        QObject::connect(palette, &ScreenshotToolPalette::recordingOpenFolderRequested, &owner,
                         [this]() { openFolder(); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingCloseRequested, &owner,
                         [this]() { close(); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingCopyGifRequested, &owner,
                         [this]() { stop(true, true, false); });
        QObject::connect(palette, &ScreenshotToolPalette::recordingCopyVideoRequested, &owner,
                         [this]() { stop(false, true, false); });
    }

    void start() {
        if (state != ScreenshotToolPalette::RecordingState::Idle || busy || startScheduled ||
            recordingSession != nullptr) {
            return;
        }
        startScheduled = true;
        QTimer::singleShot(0, &owner, [this]() {
            startScheduled = false;
            if (state != ScreenshotToolPalette::RecordingState::Idle || busy ||
                recordingSession != nullptr || !isOpen()) {
                return;
            }
            busy = true;
            syncUi();
            QDir outputDirectory(recordingDirectory());
            QDir workingDirectory(recordingWorkingDirectory());
            if (!outputDirectory.mkpath(QStringLiteral(".")) ||
                !workingDirectory.mkpath(QStringLiteral("."))) {
                busy = false;
                syncUi();
                showError(tr("Unable to create the recording directories"));
                return;
            }

            const QByteArray workingDirectoryUtf8 =
                QDir::toNativeSeparators(workingDirectory.absolutePath()).toUtf8();
            const SnowCaptureRecordingConfig config{
                captureRegion.x(),
                captureRegion.y(),
                static_cast<uint32_t>(captureRegion.width()),
                static_cast<uint32_t>(captureRegion.height()),
                kRecordingFps,
                static_cast<uint8_t>(microphoneEnabled),
                static_cast<uint8_t>(systemAudioEnabled),
                {0, 0},
                workingDirectoryUtf8.constData(),
                {},
            };
            recordingSession = snow_capture_recording_session_create(&config);
            if (recordingSession == nullptr ||
                snow_capture_recording_session_start(recordingSession) == 0) {
                const QString error = captureError();
                if (recordingSession != nullptr) {
                    snow_capture_recording_session_destroy(recordingSession);
                    recordingSession = nullptr;
                }
                busy = false;
                syncUi();
                if (areaWindow != nullptr) {
                    areaWindow->show();
                    areaWindow->raise();
                }
                if (toolbarWindow != nullptr) {
                    toolbarWindow->show();
                    toolbarWindow->raise();
                }
                showError(error);
                return;
            }

            durationMilliseconds = 0;
            state = ScreenshotToolPalette::RecordingState::Recording;
            busy = false;
            syncUi();
            if (areaWindow != nullptr) {
                areaWindow->show();
                areaWindow->raise();
            }
            if (toolbarWindow != nullptr) {
                toolbarWindow->show();
                toolbarWindow->raise();
            }
            durationTimer.start();
        });
    }

    void pause() {
        if (recordingSession == nullptr ||
            state != ScreenshotToolPalette::RecordingState::Recording || busy) {
            return;
        }
        if (snow_capture_recording_session_pause(recordingSession) == 0) {
            showError(captureError());
            return;
        }
        state = ScreenshotToolPalette::RecordingState::Paused;
        durationTimer.stop();
        syncUi();
    }

    void resume() {
        if (recordingSession == nullptr || state != ScreenshotToolPalette::RecordingState::Paused ||
            busy) {
            return;
        }
        if (snow_capture_recording_session_resume(recordingSession) == 0) {
            showError(captureError());
            return;
        }
        state = ScreenshotToolPalette::RecordingState::Recording;
        durationTimer.start();
        syncUi();
    }

    void stop(bool gif, bool copyToClipboard, bool closeAfter) {
        if (busy) {
            return;
        }
        if (state == ScreenshotToolPalette::RecordingState::Idle || recordingSession == nullptr) {
            if (closeAfter) {
                hideWindows();
            }
            return;
        }

        durationTimer.stop();
        durationMilliseconds = 0;
        busy = true;
        syncUi();
        const QString outputPath = recordingFilePath(gif);
        const QByteArray outputUtf8 = QDir::toNativeSeparators(outputPath).toUtf8();
        SnowCaptureRecordingSession* session = recordingSession;
        exportFuture = std::async(std::launch::async, [session, outputUtf8, gif]() {
            const bool ok = snow_capture_recording_session_stop_and_export(
                                session, outputUtf8.constData(), static_cast<uint8_t>(gif)) != 0;
            return std::make_pair(ok, ok ? QString() : captureError());
        });
        pendingOutputPath = outputPath;
        pendingCopyToClipboard = copyToClipboard;
        pendingCloseAfter = closeAfter;
        exportPollTimer.start();
    }

    void pollExport() {
        if (!exportFuture.valid() ||
            exportFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            return;
        }
        exportPollTimer.stop();
        const std::pair<bool, QString> result = exportFuture.get();
        const bool ok = result.first;
        const QString& error = result.second;
        if (recordingSession != nullptr) {
            snow_capture_recording_session_destroy(recordingSession);
            recordingSession = nullptr;
        }
        state = ScreenshotToolPalette::RecordingState::Idle;
        busy = false;
        durationMilliseconds = 0;
        syncUi();

        if (!ok) {
            showError(error);
            return;
        }
        if (pendingCopyToClipboard) {
            copyFileToClipboard(pendingOutputPath);
        }
        if (pendingCloseAfter) {
            hideWindows();
        }
    }

    void openFolder() {
        QDir directory(recordingDirectory());
        directory.mkpath(QStringLiteral("."));
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory.absolutePath()));
    }

    void close() {
        if (state == ScreenshotToolPalette::RecordingState::Idle) {
            hideWindows();
            return;
        }
        stop(false, false, true);
    }

    void hideWindows() {
        if (toolbarWindow != nullptr) {
            toolbarWindow->hide();
        }
        if (areaWindow != nullptr) {
            areaWindow->hide();
        }
    }

    void syncUi() {
        if (areaWindow != nullptr) {
            areaWindow->setRecordingState(state);
        }
        ScreenshotToolPalette* palette =
            toolbarWindow != nullptr ? toolbarWindow->palette() : nullptr;
        if (palette != nullptr) {
            palette->setRecordingState(state);
            palette->setRecordingDuration(durationMilliseconds);
            palette->setRecordingMicrophoneEnabled(microphoneEnabled);
            palette->setRecordingSystemAudioEnabled(systemAudioEnabled);
            palette->setRecordingBusy(busy);
        }
    }

    void updateCaptureRegion() {
        QScreen* screen = ScreenshotGeometryMapper::screenForPhysicalRect(physicalRegion);
        const QRect bounds =
            screen != nullptr ? ScreenshotGeometryMapper::physicalRectForScreen(*screen) : QRect();
        captureRegion = snow_shot::presentation::recording::videoRecordingCompatibleCaptureRegion(
            physicalRegion, bounds);
    }

    void showError(const QString& message) {
        QMessageBox::critical(toolbarWindow, tr("Video recording"),
                              message.isEmpty() ? tr("The recording operation failed") : message);
    }

    VideoRecordingController& owner;
    VideoRecordingAreaWindow* areaWindow = nullptr;
    VideoRecordingToolbarWindow* toolbarWindow = nullptr;
    SnowCaptureRecordingSession* recordingSession = nullptr;
    QRect physicalRegion;
    QRect captureRegion;
    QTimer durationTimer;
    QTimer exportPollTimer;
    std::future<std::pair<bool, QString>> exportFuture;
    ScreenshotToolPalette::RecordingState state = ScreenshotToolPalette::RecordingState::Idle;
    qint64 durationMilliseconds = 0;
    QString pendingOutputPath;
    bool microphoneEnabled = false;
    bool systemAudioEnabled = true;
    bool busy = false;
    bool startScheduled = false;
    bool pendingCopyToClipboard = false;
    bool pendingCloseAfter = false;
};

VideoRecordingController::VideoRecordingController(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this)) {}

VideoRecordingController::~VideoRecordingController() = default;

void VideoRecordingController::open(const QRect& physicalRegion) {
    m_impl->open(physicalRegion);
}

bool VideoRecordingController::isOpen() const {
    return m_impl->isOpen();
}

bool VideoRecordingController::isRecording() const {
    return m_impl->state != ScreenshotToolPalette::RecordingState::Idle;
}

void VideoRecordingController::startRecording() {
    m_impl->start();
}

void VideoRecordingController::stopRecordingAndCopyVideo() {
    m_impl->stop(false, true, false);
}
