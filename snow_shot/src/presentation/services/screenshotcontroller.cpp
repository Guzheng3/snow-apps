#include "snow_shot/presentation/screenshotcontroller.h"
#include "snow_shot/network/snowshotapiclient.h"

#include "snow_shot/presentation/screenshotcaptureruntimeadapter.h"
#include "snow_shot/presentation/screenshotcapturestate.h"
#include "snow_shot/presentation/screenshotcaptureworkflow.h"
#include "snow_shot/presentation/screenshotclipboardservice.h"
#include "snow_shot/presentation/screenshotcolorpickercontroller.h"
#include "snow_shot/presentation/screenshotdisplayconfigurationobserver.h"
#include "snow_shot/presentation/screenshotdefaultstyles.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotexportservice.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshothistoryservice.h"
#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/capturehistorytypes.h"
#include "snow_shot/presentation/screenshotintelligentselectionmodel.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotmessageservice.h"
#include "snow_shot/presentation/screenshotocrcontroller.h"
#include "snow_shot/presentation/screenshotocrrecognitionservice.h"
#include "snow_shot/presentation/settings/textrecognitionacceleration.h"
#include "snow_shot/presentation/screenshotqrrecognitionservice.h"
#include "snow_shot/presentation/screenshotselectioneditworkflow.h"
#include "snow_shot/presentation/screenshotselectionexportworkflow.h"
#include "snow_shot/presentation/screenshotselectionexportuiservices.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"
#include "snow_shot/presentation/screenshotselectionresizeworkflow.h"
#include "snow_shot/presentation/screenshotselectionsettingsstore.h"
#include "snow_shot/presentation/screenshotscrollingcapturecontroller.h"
#include "snow_shot/presentation/screenshotoverlaycoordinator.h"
#include "snow_shot/presentation/screenshotoverlayinteractionadapter.h"
#include "snow_shot/presentation/screenshotoverlayinputhandler.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"
#include "snow_shot/presentation/screenshotpresentationservices.h"
#include "snow_shot/presentation/screenshotselectorcoordinator.h"
#include "snow_shot/presentation/screenshotselectorworkflow.h"
#include "snow_shot/presentation/screenshottoolbarcommands.h"
#include "snow_shot/presentation/screenshottoolbarpresenter.h"
#include "snow_shot/presentation/screenshottoolbarwindow.h"
#include "snow_shot/presentation/screenshottoolcommandworkflow.h"
#include "snow_shot/presentation/screenshotwindowcapture.h"
#include "snow_shot/presentation/videorecordingcontroller.h"
#include "../pinned/screenshotpintoperfinstrumentation.h"

#include "snow_draw_engine_qt/snow_canvas_runtime.h"
#include "snow_draw_engine_qt/snow_canvas_widget.h"
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QPointer>
#include <QRectF>
#include <QScreen>
#include <QTimer>
#include <QPainter>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#if defined(Q_OS_WIN) || defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr auto kCopyMessageKey = "screenshot-copy";
} // namespace

struct ScreenshotController::Impl final : public ScreenshotToolbarCommandSink,
                                          public ScreenshotSelectionToolbarCommandSink {
    using CapturedDisplay = CapturedDisplayModel;

    enum class PendingSelectionAction {
        None,
        Pin,
        RecognizeText,
        Copy,
        StartVideo,
    };

    enum class AutomaticSelectionMode {
        None,
        CurrentMonitor,
        FocusedWindow,
    };

    explicit Impl(ScreenshotController& controller);
    ~Impl();

    void createPresentationInfrastructure();
    void createSelectionWorkflows();
    void createSelectorWorkflow();
    void createToolCommandWorkflow();
    void createCaptureRuntimeAdapter();
    void createCaptureWorkflow();
    void createHistoryService();
    void createDisplayConfigurationObserver();
    void createOverlayInputPipeline();
    void createToolbarCommands();
    void connectSelectorSignals();
    void shutdown();
    void startHistoryEdit(const QString& recordId);
    void handleCapturePresented();
    void invalidateDelayedCapture();
    void resetPendingCaptureRequest();
    [[nodiscard]] bool beginCapture(
        PendingSelectionAction action = PendingSelectionAction::None,
        snow_shot::storage::CaptureHistorySource historySource =
            snow_shot::storage::CaptureHistorySource::CopiedToClipboard,
        AutomaticSelectionMode automaticMode = AutomaticSelectionMode::None,
        const QPoint& automaticPhysicalPoint = QPoint());
    void handleSelectionConfirmed();
    void executeAutomaticSelection();
    void applyFocusedWindowCapture();
    [[nodiscard]] bool canBeginCapture() const;
    [[nodiscard]] ScreenshotOverlayWindow* overlayUnderCursor() const;
    void setHistoryLoadingMessageVisible(bool visible);
    [[nodiscard]] bool stopScrollingCapture(bool restoreScreenshotPresentation);
    [[nodiscard]] std::optional<quint64> beginImageExport();
    [[nodiscard]] bool imageExportCurrent(quint64 generation) const;
    [[nodiscard]] bool finishImageExport(quint64 generation);
    void hideImageExportPresentation();
    void completeScrollingResultExport(quint64 generation);
    void restoreToolUiAfterScrollingCapture(bool scrollingCaptureStopped);
    [[nodiscard]] bool resetCanvasEditingState();
    [[nodiscard]] bool prepareHistoryCandidate(std::optional<ScreenshotHistoryEntry>* candidate);

    void undoCanvasEdit() override;
    void redoCanvasEdit() override;
    void setMoveTool() override;
    void setSelectTool() override;
    void setShapeTool() override;
    void setArrowTool() override;
    void setLineTool() override;
    void setFreeDrawTool() override;
    void setHighlightTool() override;
    void setPenHighlightTool() override;
    void setSpotlightTool() override;
    void setEraserTool() override;
    void setFilterTool() override;
    void setRectangleFilterTool() override;
    void setPenFilterTool() override;
    void setWatermarkTool() override;
    void setWatermarkConfigFromToolbar(const SnowCanvasWatermarkConfig& config) override;
    void setSpotlightConfigFromToolbar(const SnowCanvasSpotlightConfig& config) override;
    void previewSpotlightFromToolbar(const SnowCanvasSpotlightConfig& config) override;
    void previewWatermarkFromToolbar(const SnowCanvasWatermarkConfig& config) override;
    void setFilterStyleFromToolbar(const SnowCanvasFilterStyle& style, quint32 properties) override;
    void setTextTool() override;
    void setSerialNumberTool() override;
    void setOcrTool() override;
    void setTableTool() override;
    void setQrTool() override;
    void mergeTableSelection() override;
    void splitTableSelection() override;
    void resetTable() override;
    void toggleTextEditing() override;
    void resetTextEditing() override;
    void applyTextFormatting(const QString& value) override;
    void applyTextPunctuation(const QString& value) override;
    void startScrollingScreenshot() override;
    void setScrollingScreenshotRecognitionMode(ScreenshotScrollingRecognitionMode mode) override;
    void pinSelectionToScreen() override;
    void cancelCapture() override;
    void copySelectionToClipboard() override;
    void copySelectionToClipboardWithSource(
        snow_shot::storage::CaptureHistorySource historySource);
    void startVideoRecording() override;
    void setShapeStyleFromToolbar(const SnowCanvasShapeStyle& style, quint32 properties,
                                  SnowCanvasShapeKind kind) override;
    void setTextStyleFromToolbar(const SnowCanvasTextStyle& style) override;
    void setSerialNumberStyleFromToolbar(const SnowCanvasSerialNumberStyle& style) override;
    void decrementSelectedSerialNumbers() override;
    void incrementSelectedSerialNumbers() override;
    void createTextForSelectedSerialNumber() override;
    void reorderSelectedElements(SnowCanvasSelectionOrder order) override;
    void setSelectedElementsOpacity(qreal opacity) override;
    void duplicateSelectedElements() override;
    void deleteSelectedElements() override;
    void repositionToolbarForContentChange() override;
    void toggleSelectionAspectRatioLockFromToolbar() override;
    void openSelectionResizeModalFromToolbar() override;
    void hideColorPickersForScreenshotUi() override;
    void adjustSelectionFromToolbar(int minDx, int minDy, int maxDx, int maxDy) override;
    void setSelectionCornerRadiusFromToolbar(int radius) override;
    void setSelectionShadowWidthFromToolbar(int shadowWidth) override;
    void setSelectionToolbarHovered(bool hovered) override;

    ScreenshotController& owner;
    ScreenshotSelectorCoordinator* m_selectorCoordinator = nullptr;
    ScreenshotCaptureState m_captureState;
    std::unique_ptr<ScreenshotOverlayEventAdapter> m_overlayEventAdapter;
    std::unique_ptr<ScreenshotOverlayCoordinator> m_overlayCoordinator;
    std::unique_ptr<ScreenshotPresentationServices> m_presentationServices;
    std::unique_ptr<ScreenshotCaptureRuntimeAdapter> m_captureRuntime;
    std::unique_ptr<ScreenshotCaptureWorkflow> m_captureWorkflow;
    std::unique_ptr<ScreenshotDisplayConfigurationObserver> m_displayConfigurationObserver;
    std::unique_ptr<ScreenshotHistoryService> m_historyService;
    std::unique_ptr<ScreenshotSelectionSettingsStore> m_selectionSettings;
    std::unique_ptr<ScreenshotExportService> m_exportService;
    std::unique_ptr<ScreenshotSelectionExportUiServices> m_selectionExportUiServices;
    std::unique_ptr<ScreenshotSelectionExportWorkflow> m_selectionExportWorkflow;
    std::unique_ptr<ScreenshotSelectionEditWorkflow> m_selectionEditWorkflow;
    std::unique_ptr<ScreenshotColorPickerController> m_colorPickerController;
    std::unique_ptr<ScreenshotToolbarPresenter> m_toolbarPresenter;
    std::unique_ptr<ScreenshotToolCommandWorkflow> m_toolCommandWorkflow;
    std::unique_ptr<ScreenshotOcrRecognitionService> m_ocrRecognition;
    std::unique_ptr<ScreenshotQrRecognitionService> m_qrRecognition;
    std::unique_ptr<ScreenshotMessageService> m_messages;
    std::unique_ptr<SnowShotApiClient> m_tableRecognition;
    std::unique_ptr<ScreenshotOcrController> m_ocrController;
    std::unique_ptr<ScreenshotSelectionResizeWorkflow> m_selectionResizeWorkflow;
    std::unique_ptr<ScreenshotScrollingCaptureController> m_scrollingCaptureController;
    std::unique_ptr<ScreenshotOverlayInputHandler> m_overlayInputHandler;
    std::unique_ptr<ScreenshotSelectorWorkflow> m_selectorWorkflow;
    QPointer<ScreenshotOverlayWindow> m_historyLoadingMessageOwner;
    QString m_pendingHistoryEditRecordId;
    quint64 m_imageExportGeneration = 0;
    bool m_imageExportInFlight = false;
    quint64 m_delayedCaptureGeneration = 0;
    PendingSelectionAction m_pendingSelectionAction = PendingSelectionAction::None;
    snow_shot::storage::CaptureHistorySource m_pendingHistorySource =
        snow_shot::storage::CaptureHistorySource::CopiedToClipboard;
    AutomaticSelectionMode m_automaticSelectionMode = AutomaticSelectionMode::None;
    QPoint m_automaticPhysicalPoint;
    std::optional<ScreenshotWindowCaptureFrame> m_focusedWindowCapture;
    SnowCanvasRuntime m_canvasRuntime;
    ScreenshotGeometryMapper m_geometry;
    ScreenshotDisplaySession m_displaySession;
    ScreenshotInteractionState m_interaction;
    ScreenshotSelectionModel m_selection;
    ScreenshotIntelligentSelectionModel m_intelligentSelection;
    std::unique_ptr<VideoRecordingController> m_videoRecordingController;
};

ScreenshotController::Impl::Impl(ScreenshotController& controller)
    : owner(controller), m_canvasRuntime(SnowCanvasRuntimeConfig{
                             snow_shot::presentation::screenshotCanvasStyleDefaults()}) {
    createPresentationInfrastructure();
    createSelectionWorkflows();
    createSelectorWorkflow();
    createToolCommandWorkflow();
    createCaptureRuntimeAdapter();
    createCaptureWorkflow();
    createHistoryService();
    createDisplayConfigurationObserver();
    createOverlayInputPipeline();
    createToolbarCommands();
    connectSelectorSignals();
}

void ScreenshotController::Impl::createHistoryService() {
    m_historyService = std::make_unique<ScreenshotHistoryService>(
        ScreenshotHistoryServiceContext{
            m_displaySession,
            m_canvasRuntime,
            m_selection,
            m_interaction,
            m_intelligentSelection,
            [this]() {
                if (m_ocrController != nullptr) {
                    m_ocrController->invalidateSession();
                }
                if (m_overlayCoordinator != nullptr) {
                    m_overlayCoordinator->applyDisplayModels(m_displaySession);
                }
                const bool smartSelecting = m_interaction.intelligentSelecting();
                if (smartSelecting && m_overlayCoordinator != nullptr) {
                    m_overlayCoordinator->setCanvasInteractionEnabled(m_displaySession, false);
                }
                if (!smartSelecting) {
                    setMoveTool();
                }
                if (m_overlayCoordinator != nullptr) {
                    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
                        toolbar->setActiveTool(ScreenshotToolPalette::Tool::Move);
                    }
                }
                if (m_presentationServices != nullptr) {
                    if (smartSelecting) {
                        m_presentationServices->hideToolbar();
                    }
                    m_presentationServices->updateOverlayState();
                    if (smartSelecting) {
                        m_presentationServices->updateOverlayCursors();
                    } else {
                        m_presentationServices->showToolbar();
                    }
                }
                m_colorPickerController->updateAtCurrentCursor(
                    m_presentationServices->colorPickerContext());
            },
            [this](bool loading) { setHistoryLoadingMessageVisible(loading); },
            [this]() {
                if (m_selectorWorkflow == nullptr) {
                    return;
                }
                static_cast<void>(m_selectorWorkflow->updateSelectionAt(
                    m_geometry.physicalPositionForLogicalPoint(m_displaySession, QCursor::pos())));
            },
        },
        snow_shot::storage::ApplicationStorage::instance().captureHistory());
}

ScreenshotOverlayWindow* ScreenshotController::Impl::overlayUnderCursor() const {
    const QPoint cursorPosition = QCursor::pos();
    ScreenshotOverlayWindow* result = nullptr;
    m_displaySession.forEachActiveOverlay(
        [&result, &cursorPosition](qsizetype, const CapturedDisplayModel& display,
                                   ScreenshotOverlayWindow* overlay) {
            if (result == nullptr && overlay != nullptr && overlay->isVisible() &&
                display.logicalRect.contains(cursorPosition, false)) {
                result = overlay;
            }
        });
    return result;
}

void ScreenshotController::Impl::setHistoryLoadingMessageVisible(bool visible) {
    if (!visible) {
        if (m_historyLoadingMessageOwner != nullptr) {
            m_historyLoadingMessageOwner->setHistoryLoadingVisible(false);
            m_historyLoadingMessageOwner = nullptr;
        }
        return;
    }

    ScreenshotOverlayWindow* messageOwner = overlayUnderCursor();
    if (messageOwner == nullptr) {
        if (m_historyLoadingMessageOwner != nullptr) {
            m_historyLoadingMessageOwner->setHistoryLoadingVisible(false);
            m_historyLoadingMessageOwner = nullptr;
        }
        return;
    }
    if (m_historyLoadingMessageOwner != nullptr && m_historyLoadingMessageOwner != messageOwner) {
        m_historyLoadingMessageOwner->setHistoryLoadingVisible(false);
    }
    messageOwner->setHistoryLoadingVisible(true);
    m_historyLoadingMessageOwner = messageOwner;
}

void ScreenshotController::Impl::createPresentationInfrastructure() {
    m_overlayEventAdapter = std::make_unique<ScreenshotOverlayEventAdapter>();
    m_overlayCoordinator =
        std::make_unique<ScreenshotOverlayCoordinator>(*m_overlayEventAdapter, m_canvasRuntime);
    m_messages = std::make_unique<ScreenshotMessageService>(
        m_displaySession, m_geometry, m_selection, [this]() {
            return m_overlayCoordinator != nullptr ? m_overlayCoordinator->toolbar() : nullptr;
        });
    m_colorPickerController = std::make_unique<ScreenshotColorPickerController>(
        *m_overlayCoordinator, m_geometry, m_displaySession);
    m_toolbarPresenter = std::make_unique<ScreenshotToolbarPresenter>(*m_overlayCoordinator,
                                                                      m_geometry, m_displaySession);
    m_scrollingCaptureController = std::make_unique<ScreenshotScrollingCaptureController>(
        ScreenshotScrollingCaptureControllerContext{
            m_displaySession,
            m_geometry,
            *m_overlayCoordinator,
        },
        &owner);
    m_selectorCoordinator = new ScreenshotSelectorCoordinator(&owner);
    m_videoRecordingController = std::make_unique<VideoRecordingController>(&owner);
    m_selectionSettings = std::make_unique<ScreenshotSelectionSettingsStore>();
    m_presentationServices =
        std::make_unique<ScreenshotPresentationServices>(ScreenshotPresentationServicesContext{
            m_captureState,
            *m_overlayCoordinator,
            *m_toolbarPresenter,
            m_geometry,
            m_displaySession,
            m_interaction,
            m_selection,
        });
    m_ocrRecognition = std::make_unique<ScreenshotOcrRecognitionService>(
        []() {
            auto& storage = snow_shot::storage::ApplicationStorage::instance();
            return snow_shot::presentation::settings::directMlTextRecognitionSupported() &&
                   storage.configuration()
                       .value(QStringLiteral("text_recognition/direct_ml_acceleration"))
                       .toBool();
        },
        &owner);
    m_qrRecognition = std::make_unique<ScreenshotQrRecognitionService>(&owner);
    QString tableApiUrl = QStringLiteral(SNOW_SHOT_API_BASE_URL);
    const QString runtimeTableApiUrl =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("SNOW_SHOT_API_BASE_URL"));
    if (!runtimeTableApiUrl.trimmed().isEmpty()) {
        tableApiUrl = runtimeTableApiUrl;
    }
    m_tableRecognition = std::make_unique<SnowShotApiClient>(tableApiUrl, &owner);
    m_ocrController =
        std::make_unique<ScreenshotOcrController>(ScreenshotOcrControllerContext{
                                                      m_captureState,
                                                      m_interaction,
                                                      m_selection,
                                                      m_displaySession,
                                                      m_geometry,
                                                       *m_overlayCoordinator,
                                                       *m_ocrRecognition,
                                                       *m_qrRecognition,
                                                       m_tableRecognition.get(),
                                                      [this]() { m_colorPickerController->hide(); },
                                                      [this]() { cancelCapture(); },
                                                  },
                                                  &owner);
}

void ScreenshotController::Impl::createSelectionWorkflows() {
    m_exportService = std::make_unique<ScreenshotExportService>(ScreenshotExportServiceContext{
        m_displaySession,
        m_canvasRuntime,
        m_geometry,
    });
    m_selectionExportUiServices = std::make_unique<ScreenshotSelectionExportUiServices>(
        m_canvasRuntime, m_ocrRecognition.get(), m_qrRecognition.get(),
        m_tableRecognition.get());
    m_selectionExportWorkflow = std::make_unique<ScreenshotSelectionExportWorkflow>(
        ScreenshotSelectionExportWorkflowContext{
            m_captureState,
            m_geometry,
            m_selection,
            *m_exportService,
            *m_selectionExportUiServices,
            *m_selectionSettings,
            owner,
        });
    m_selectionResizeWorkflow =
        std::make_unique<ScreenshotSelectionResizeWorkflow>(*m_selectionSettings);
    m_selectionEditWorkflow =
        std::make_unique<ScreenshotSelectionEditWorkflow>(ScreenshotSelectionEditWorkflowContext{
            owner,
            m_captureState,
            m_displaySession,
            m_geometry,
            m_interaction,
            m_selection,
            ScreenshotSelectionEditUiActions{
                [this]() { m_presentationServices->updateOverlayState(); },
                [this]() { m_presentationServices->showSelectionToolbar(); },
                [this]() { m_presentationServices->moveToolbar(); },
                [this]() { m_presentationServices->repositionToolbarForContentChange(); },
                [this]() { m_presentationServices->showToolbar(); },
                [this](QObject* modalParent, const ScreenshotSelectionResizeRequest& request,
                       ScreenshotApplySelectionCallback applySelection) {
                    return m_selectionResizeWorkflow->open(modalParent, request,
                                                           std::move(applySelection));
                },
                [this]() { m_colorPickerController->hide(); },
                [this](bool suppressed) { m_colorPickerController->setSuppressed(suppressed); },
            },
        });
}

void ScreenshotController::Impl::createSelectorWorkflow() {
    m_selectorWorkflow =
        std::make_unique<ScreenshotSelectorWorkflow>(ScreenshotSelectorWorkflowContext{
            m_captureState,
            *m_selectorCoordinator,
            *m_overlayCoordinator,
            m_displaySession,
            m_geometry,
            m_interaction,
            m_selection,
            m_intelligentSelection,
            ScreenshotSelectorPresentationCallbacks{
                [this]() { m_presentationServices->updateOverlayState(); },
                [this]() {
                    m_colorPickerController->updateAtCurrentCursor(
                        m_presentationServices->colorPickerContext());
                },
                [this]() { m_presentationServices->hideToolbar(); },
                [this]() { m_presentationServices->updateOverlayCursors(); },
                [this](quint64 sessionId) {
                    if (m_captureWorkflow != nullptr) {
                        m_captureWorkflow->handleInitialSmartSelectionResolved(sessionId);
                    }
                },
            },
        });
}

void ScreenshotController::Impl::createToolCommandWorkflow() {
    m_toolCommandWorkflow =
        std::make_unique<ScreenshotToolCommandWorkflow>(ScreenshotToolCommandWorkflowContext{
            m_captureState,
            ScreenshotToolCommandActions{
                [this]() { return m_selectorCoordinator->ready(); },
                [this]() { m_selectorWorkflow->startRefresh(); },
                [this](const QPoint& physicalPoint) {
                    static_cast<void>(m_selectorWorkflow->updateSelectionAt(physicalPoint));
                },
                [this]() { m_selectorWorkflow->clearSelection(); },
                [this](bool enabled) {
                    m_overlayCoordinator->setCanvasInteractionEnabled(m_displaySession, enabled);
                },
                [this](SnowCanvasTool tool) {
                    m_overlayCoordinator->setCanvasTool(m_displaySession, tool);
                },
                [this](SnowCanvasShapeStyle* outStyle) {
                    return m_overlayCoordinator->tryCurrentRectangleStyle(m_displaySession,
                                                                          outStyle);
                },
                [this](SnowCanvasStyleToolbarState* outState) {
                    return m_overlayCoordinator->tryCurrentStyleToolbarState(m_displaySession,
                                                                             outState);
                },
                [this](const SnowCanvasShapeStyle& style, quint32 properties,
                       SnowCanvasShapeKind kind) {
                    m_overlayCoordinator->setShapeStylePatch(m_displaySession, style, properties,
                                                             kind);
                },
                [this](const SnowCanvasFilterStyle& style, quint32 properties) {
                    m_overlayCoordinator->setFilterStyle(m_displaySession, style, properties);
                },
                [this](const SnowCanvasWatermarkConfig& config) {
                    m_overlayCoordinator->setWatermarkConfig(m_displaySession, config);
                },
                [this](const SnowCanvasSpotlightConfig& config) {
                    m_overlayCoordinator->setSpotlightConfig(m_displaySession, config);
                },
                [this](const SnowCanvasTextStyle& style) {
                    m_overlayCoordinator->setTextStyle(m_displaySession, style);
                },
                [this](const SnowCanvasSerialNumberStyle& style) {
                    m_overlayCoordinator->setSerialNumberStyle(m_displaySession, style);
                },
                [this](qint64 delta) {
                    m_overlayCoordinator->adjustSelectedSerialNumbers(m_displaySession, delta);
                },
                [this]() {
                    m_overlayCoordinator->createTextForSelectedSerialNumber(m_displaySession);
                },
                [this](int direction) {
                    return m_overlayCoordinator->stepToolbarStrokeWidth(direction);
                },
                [this]() { m_presentationServices->updateOverlayState(); },
                [this]() { m_presentationServices->updateOverlayCursors(); },
                [this]() { m_presentationServices->raiseToolbarForCanvasInteraction(); },
            },
            m_displaySession,
            m_geometry,
            m_interaction,
            m_selection,
            m_intelligentSelection,
        });
}

void ScreenshotController::Impl::createCaptureRuntimeAdapter() {
    m_captureRuntime =
        std::make_unique<ScreenshotCaptureRuntimeAdapter>(ScreenshotCaptureRuntimeAdapterContext{
            *m_selectorCoordinator,
            *m_selectorWorkflow,
            *m_overlayCoordinator,
            *m_colorPickerController,
            m_canvasRuntime,
        });
}

void ScreenshotController::Impl::createCaptureWorkflow() {
    m_captureWorkflow =
        std::make_unique<ScreenshotCaptureWorkflow>(ScreenshotCaptureWorkflowContext{
            m_captureState,
            *m_captureRuntime,
            m_geometry,
            m_displaySession,
            m_interaction,
            m_selection,
            m_intelligentSelection,
            ScreenshotCapturePresentationCallbacks{
                [this]() { m_presentationServices->hideToolbar(); },
                [this]() { m_presentationServices->updateOverlayState(); },
                [this]() {
                    m_colorPickerController->updateAtCurrentCursor(
                        m_presentationServices->colorPickerContext());
                },
                [this]() { handleCapturePresented(); },
            },
            [this]() {
                m_pendingHistoryEditRecordId.clear();
                resetPendingCaptureRequest();
                if (m_ocrController != nullptr) {
                    m_ocrController->invalidateSession();
                }
            },
        });
}

void ScreenshotController::Impl::startHistoryEdit(const QString& recordId) {
    if (recordId.isEmpty()) {
        return;
    }
    // An explicit history edit supersedes a delayed shortcut that has not fired yet.
    invalidateDelayedCapture();
    const bool idleSession = m_captureState.sessionState == ScreenshotSessionState::IdleCold ||
                             m_captureState.sessionState == ScreenshotSessionState::IdlePrepared;
    if (!idleSession || m_captureState.captureInProgress ||
        !m_interaction.inactive() || m_captureWorkflow == nullptr || m_historyService == nullptr) {
        return;
    }

    resetPendingCaptureRequest();
    m_pendingHistoryEditRecordId = recordId;
    m_ocrController->invalidateSession();
    m_historyService->resetCaptureNavigation();
    m_captureWorkflow->startCapture();
}

void ScreenshotController::Impl::handleCapturePresented() {
    if (!m_pendingHistoryEditRecordId.isEmpty()) {
        const QString recordId = std::exchange(m_pendingHistoryEditRecordId, QString());
        if (m_historyService != nullptr) {
            static_cast<void>(m_historyService->navigateToRecord(recordId));
        }
        return;
    }
    executeAutomaticSelection();
}

void ScreenshotController::Impl::createDisplayConfigurationObserver() {
    m_displayConfigurationObserver = std::make_unique<ScreenshotDisplayConfigurationObserver>(
        [this]() {
            static_cast<void>(stopScrollingCapture(false));
            if (m_captureWorkflow != nullptr) {
                m_captureWorkflow->handleDisplayConfigurationChanged();
            }
        },
        &owner);
    m_displayConfigurationObserver->connectApplicationSignals(qApp);
    m_displayConfigurationObserver->observeCurrentScreens();
}

void ScreenshotController::Impl::createOverlayInputPipeline() {
    m_overlayInputHandler = std::make_unique<
        ScreenshotOverlayInputHandler>(ScreenshotOverlayInputHandlerContext{
        m_captureState,
        m_interaction,
        m_selection,
        m_intelligentSelection,
        m_geometry,
        m_displaySession,
        ScreenshotOverlayInputActions{
            [this](const QPoint& physicalPoint) {
                return m_selectorWorkflow->returnToSelection(physicalPoint);
            },
            [this](const QPoint& physicalPoint) {
                static_cast<void>(m_selectorWorkflow->requestHitTest(physicalPoint));
            },
            [this]() { m_selectorCoordinator->resetHitTestState(); },
            [this](ScreenshotOverlayWindow* overlay, ScreenshotSelectionDragMode dragMode) {
                m_overlayCoordinator->setOverlayCursor(overlay, dragMode);
            },
            [this]() { m_presentationServices->hideMainToolbar(); },
            [this]() { m_presentationServices->updateOverlayState(); },
            [this]() { m_presentationServices->showToolbar(); },
            [this]() { m_presentationServices->showSelectionToolbar(); },
            [this]() { cancelCapture(); },
            [this](int delta) { return m_toolCommandWorkflow->stepStrokeWidth(delta); },
            [this](int delta) { return m_overlayCoordinator->stepToolbarSelectionOpacity(delta); },
            [this](int delta) { return m_overlayCoordinator->stepToolbarSpotlightOpacity(delta); },
            [this](int delta) { return m_overlayCoordinator->stepToolbarFilterIntensity(delta); },
            [this](int delta) {
                return m_overlayCoordinator->stepToolbarPenFilterStrokeWidth(delta);
            },
            [this](int delta) { return m_overlayCoordinator->stepToolbarWatermarkFontSize(delta); },
            [this]() { copySelectionToClipboard(); },
            [this]() {
                return m_historyService != nullptr && m_historyService->navigatePrevious();
            },
            [this]() { return m_historyService != nullptr && m_historyService->navigateNext(); },
            [this]() {
                return m_historyService != nullptr && m_historyService->returnToCurrentScreenshot();
            },
            [this](ScreenshotOverlayWindow* overlay, const QPointF& localPosition) {
                m_colorPickerController->updateForOverlay(
                    overlay, localPosition, m_presentationServices->colorPickerContext());
            },
            [this](const QPointF& virtualPosition) {
                m_colorPickerController->updateForSelectionDrag(
                    virtualPosition, m_presentationServices->colorPickerContext());
            },
            [this]() {
                return m_colorPickerController->copyColorToClipboard(
                    m_presentationServices->colorPickerContext());
            },
            [this]() {
                return m_colorPickerController->cycleFormat(
                    m_presentationServices->colorPickerContext());
            },
            [this](int dx, int dy) {
                return m_colorPickerController->moveCursor(
                    dx, dy, m_presentationServices->colorPickerContext());
            },
            [this]() { handleSelectionConfirmed(); },
        },
    });
    m_overlayEventAdapter->setEventTargets(*m_overlayInputHandler, [this]() {
        m_presentationServices->raiseToolbarForCanvasInteraction();
    });
}

void ScreenshotController::Impl::createToolbarCommands() {
    m_overlayCoordinator->setToolbarCommandSinks(*this, *this);
}

void ScreenshotController::Impl::undoCanvasEdit() {
    if (m_ocrController != nullptr && m_ocrController->tableModeActive()) {
        m_ocrController->undoTableEdit();
        return;
    }
    if (m_ocrController != nullptr && m_ocrController->qrModeActive()) {
        return;
    }
    if (m_ocrController != nullptr && m_ocrController->editing()) {
        m_ocrController->undoTextEdit();
        return;
    }
    m_overlayCoordinator->undoCanvasEdit();
}

void ScreenshotController::Impl::redoCanvasEdit() {
    if (m_ocrController != nullptr && m_ocrController->tableModeActive()) {
        m_ocrController->redoTableEdit();
        return;
    }
    if (m_ocrController != nullptr && m_ocrController->qrModeActive()) {
        return;
    }
    if (m_ocrController != nullptr && m_ocrController->editing()) {
        m_ocrController->redoTextEdit();
        return;
    }
    m_overlayCoordinator->redoCanvasEdit();
}

void ScreenshotController::Impl::connectSelectorSignals() {
    QObject::connect(m_selectorCoordinator, &ScreenshotSelectorCoordinator::refreshFinished, &owner,
                     [this](bool ok) { m_selectorWorkflow->handleRefreshFinished(ok); });
    QObject::connect(m_selectorCoordinator, &ScreenshotSelectorCoordinator::hitTestFinished, &owner,
                     [this](bool ok, const QVector<QRectF>& hitRects) {
                         m_selectorWorkflow->handleHitTestFinished(ok, hitRects);
                     });
}

void ScreenshotController::Impl::setMoveTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    static_cast<void>(resetCanvasEditingState());
    m_toolCommandWorkflow->setMoveTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setSelectTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setSelectTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setShapeTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setShapeTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setArrowTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setArrowTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setTextTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setTextTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setSerialNumberTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setSerialNumberTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setOcrTool() {
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    if (resetCanvasEditingState()) {
        m_interaction.setCanvasTool(ScreenshotActiveTool::Select);
    }
    m_ocrController->activate();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setTableTool() {
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_ocrController->activateTable();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setQrTool() {
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_ocrController->activateQr();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::mergeTableSelection() {
    m_ocrController->mergeTableSelection();
}

void ScreenshotController::Impl::splitTableSelection() {
    m_ocrController->splitTableSelection();
}

void ScreenshotController::Impl::resetTable() {
    m_ocrController->resetTable();
}

void ScreenshotController::Impl::toggleTextEditing() {
    if (m_ocrController->editing()) {
        m_ocrController->endTextEditing();
    } else {
        m_ocrController->beginTextEditing();
    }
    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
        toolbar->setTextEditingState(m_ocrController->hasTextResult(), m_ocrController->editing());
    }
}

void ScreenshotController::Impl::resetTextEditing() {
    m_ocrController->resetTextEditing();
    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
        toolbar->setTextEditingState(m_ocrController->hasTextResult(), m_ocrController->editing());
    }
}

void ScreenshotController::Impl::applyTextFormatting(const QString& value) {
    if (value == QStringLiteral("keep")) {
        m_ocrController->beginTextEditing();
        m_ocrController->resetTextEditing();
    } else if (value == QStringLiteral("remove")) {
        m_ocrController->applyRemoveLineBreaks();
    }
    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
        toolbar->clearTextTransformSelections();
        toolbar->setTextEditingState(m_ocrController->hasTextResult(), m_ocrController->editing());
    }
}

void ScreenshotController::Impl::applyTextPunctuation(const QString& value) {
    if (value == QStringLiteral("half")) {
        m_ocrController->applyHalfWidthPunctuation();
    } else if (value == QStringLiteral("full")) {
        m_ocrController->applyFullWidthPunctuation();
    }
    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
        toolbar->clearTextTransformSelections();
        toolbar->setTextEditingState(m_ocrController->hasTextResult(), m_ocrController->editing());
    }
}

bool ScreenshotController::Impl::stopScrollingCapture(bool restoreScreenshotPresentation) {
    if (m_scrollingCaptureController == nullptr || !m_scrollingCaptureController->active()) {
        return false;
    }

    m_scrollingCaptureController->stop(restoreScreenshotPresentation);
    if (m_overlayCoordinator != nullptr) {
        if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
            toolbar->setScrollingScreenshotMode(false);
        }
    }
    return true;
}

std::optional<quint64> ScreenshotController::Impl::beginImageExport() {
    if (m_imageExportInFlight) {
        return std::nullopt;
    }
    m_imageExportInFlight = true;
    return ++m_imageExportGeneration;
}

bool ScreenshotController::Impl::finishImageExport(quint64 generation) {
    if (!m_imageExportInFlight || generation != m_imageExportGeneration) {
        return false;
    }
    m_imageExportInFlight = false;
    return true;
}

bool ScreenshotController::Impl::imageExportCurrent(quint64 generation) const {
    return m_imageExportInFlight && generation == m_imageExportGeneration;
}

void ScreenshotController::Impl::hideImageExportPresentation() {
    if (m_colorPickerController != nullptr) {
        m_colorPickerController->hide();
    }
    if (m_toolbarPresenter != nullptr) {
        m_toolbarPresenter->hideSelectionToolbar();
    }
    if (m_overlayCoordinator != nullptr) {
        m_overlayCoordinator->hideOverlayWindowsImmediately(m_displaySession);
    }
}

void ScreenshotController::Impl::completeScrollingResultExport(quint64 generation) {
    if (!finishImageExport(generation)) {
        return;
    }
    static_cast<void>(stopScrollingCapture(false));
    m_ocrController->invalidateSession();
    m_captureWorkflow->cancelCapture();
}

void ScreenshotController::Impl::restoreToolUiAfterScrollingCapture(bool scrollingCaptureStopped) {
    if (!scrollingCaptureStopped) {
        return;
    }

    m_presentationServices->updateOverlayState();
    m_presentationServices->showToolbar();
    m_presentationServices->showSelectionToolbar();
}

void ScreenshotController::Impl::startScrollingScreenshot() {
    m_ocrController->deactivate();
    if (m_scrollingCaptureController == nullptr || m_scrollingCaptureController->active() ||
        !m_selection.hasPixelSelection()) {
        return;
    }

    const QRect selection = m_selection.pixelSelection();
    if (!m_scrollingCaptureController->start(selection,
                                             ScreenshotScrollingRecognitionMode::Vertical)) {
        if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
            toolbar->setScrollingScreenshotMode(false);
        }
        return;
    }
    static_cast<void>(resetCanvasEditingState());

    m_interaction.enterScrollingCapture();
    m_captureState.sessionState = ScreenshotSessionState::Editing;
    m_colorPickerController->hide();
    m_toolbarPresenter->hideSelectionToolbar();
    if (ScreenshotToolbarWindow* toolbar = m_overlayCoordinator->toolbar()) {
        toolbar->setScrollingScreenshotMode(true);
    }
}

void ScreenshotController::Impl::pinSelectionToScreen() {
    if (m_scrollingCaptureController != nullptr && m_scrollingCaptureController->active()) {
        const QSize sourceSize = m_scrollingCaptureController->trimmedSize();
        if (sourceSize.isEmpty()) {
            return;
        }
        SNOW_SHOT_PIN_PERF_BEGIN("scrolling-selection", sourceSize.width(), sourceSize.height());
        SNOW_SHOT_PIN_PERF_MILESTONE("controller.enter");
        const QRect selection = m_scrollingCaptureController->canvasSelection();
        const CapturedDisplayModel* display = m_geometry.displayForCanvasPoint(
            m_displaySession, ScreenshotHalfOpenRect::fromRect(selection).center());
        if (display == nullptr || display->screen == nullptr) {
            SNOW_SHOT_PIN_PERF_FINISH(false);
            return;
        }
        const QPointer<ScreenshotController> receiver(&owner);
        const QPointer<QScreen> targetScreen(display->screen);
        const QString targetScreenName = display->name;
        const QRect targetPhysicalRect = display->physicalRect;
        const std::optional<quint64> exportGeneration = beginImageExport();
        if (!exportGeneration.has_value()) {
            SNOW_SHOT_PIN_PERF_FINISH(false);
            return;
        }
        const bool scheduled = m_scrollingCaptureController->requestTrimmedImage(
            [receiver, targetScreen, targetScreenName, targetPhysicalRect,
             generation = *exportGeneration](QImage image) {
                if (receiver.isNull() || receiver->m_impl == nullptr) {
                    SNOW_SHOT_PIN_PERF_FINISH(false);
                    return;
                }
                if (!receiver->m_impl->imageExportCurrent(generation)) {
                    SNOW_SHOT_PIN_PERF_FINISH(false);
                    return;
                }
                QScreen* resolvedScreen = targetScreen;
                if (resolvedScreen == nullptr ||
                    !QGuiApplication::screens().contains(resolvedScreen)) {
                    resolvedScreen = ScreenshotGeometryMapper::screenForCaptureDisplay(
                        targetScreenName, targetPhysicalRect);
                }
                const ScreenshotPinnedImageFit fit =
                    resolvedScreen != nullptr
                        ? ScreenshotGeometryMapper::fitImageToAvailableGeometry(
                              image.size(), resolvedScreen->availableGeometry(),
                              resolvedScreen->geometry(),
                              ScreenshotGeometryMapper::physicalRectForScreen(*resolvedScreen), 16)
                        : ScreenshotPinnedImageFit{};
                const bool presented =
                    !image.isNull() && fit.valid &&
                    receiver->m_impl->m_selectionExportUiServices->presentPinnedImage(
                        image, resolvedScreen, fit.nativeGeometry, fit.fullResolutionSize);
                SNOW_SHOT_PIN_PERF_MILESTONE("controller.presentation_complete");
                SNOW_SHOT_PIN_PERF_FINISH(presented);
                if (!presented) {
                    qWarning("Scrolling screenshot pin export failed");
                    receiver->m_impl->completeScrollingResultExport(generation);
                    return;
                }
                receiver->m_impl->completeScrollingResultExport(generation);
            });
        if (!scheduled) {
            SNOW_SHOT_PIN_PERF_FINISH(false);
            static_cast<void>(finishImageExport(*exportGeneration));
            qWarning("Failed to schedule scrolling screenshot pin export");
            return;
        }
        hideImageExportPresentation();
        SNOW_SHOT_PIN_PERF_MILESTONE("controller.presentation_hidden");
        return;
    }
    const QRect perfSelection = m_selection.pixelSelection();
    SNOW_SHOT_PIN_PERF_BEGIN("normal-selection", perfSelection.width(), perfSelection.height());
    SNOW_SHOT_PIN_PERF_MILESTONE("controller.enter");
    SNOW_SHOT_PIN_PERF_SCOPE("controller.pin_selection");
    const bool historyEligible = m_interaction.activeTool() != ScreenshotActiveTool::Ocr &&
                                 m_interaction.activeTool() != ScreenshotActiveTool::Table &&
                                 m_interaction.activeTool() != ScreenshotActiveTool::Qr;
    m_ocrController->deactivate();
    SNOW_SHOT_PIN_PERF_MILESTONE("controller.ocr_deactivated");
    const std::optional<quint64> exportGeneration = beginImageExport();
    if (!exportGeneration.has_value()) {
        SNOW_SHOT_PIN_PERF_FINISH(false);
        return;
    }
    const bool shouldSnapshotHistory =
        historyEligible && m_historyService != nullptr && resetCanvasEditingState();
    auto historyCandidate = std::make_shared<std::optional<ScreenshotHistoryEntry>>();
    const QPointer<ScreenshotController> receiver(&owner);
    const bool scheduled = m_selectionExportWorkflow->pinSelectionToScreen(
        [receiver, generation = *exportGeneration]() {
            return !receiver.isNull() && receiver->m_impl != nullptr &&
                   receiver->m_impl->imageExportCurrent(generation);
        },
         [receiver, generation = *exportGeneration, historyCandidate](bool success) mutable {
            SNOW_SHOT_PIN_PERF_FINISH(success);
            if (receiver.isNull() || receiver->m_impl == nullptr ||
                !receiver->m_impl->finishImageExport(generation)) {
                return;
            }
            if (success && historyCandidate->has_value() &&
                receiver->m_impl->m_historyService != nullptr) {
                historyCandidate->value().source =
                    snow_shot::storage::CaptureHistorySource::PinnedToScreen;
                receiver->m_impl->m_historyService->commit(
                    std::move(historyCandidate->value()));
            } else if (!success) {
                qWarning("Screenshot pin export failed");
            }
            if (receiver->m_impl->m_historyService != nullptr) {
                receiver->m_impl->m_historyService->resetCaptureNavigation();
            }
            receiver->m_impl->m_ocrController->invalidateSession();
            receiver->m_impl->m_captureWorkflow->cancelCapture();
        });
    if (!scheduled) {
        SNOW_SHOT_PIN_PERF_FINISH(false);
        static_cast<void>(finishImageExport(*exportGeneration));
        qWarning("Failed to schedule screenshot pin export");
        m_captureWorkflow->cancelCapture();
        return;
    }
    // Queue the immutable request before touching the live capture presentation.
    // Its callback runs after this event-loop turn, once hide and history work settle.
    SNOW_SHOT_PIN_PERF_MILESTONE("controller.export_scheduled");
    hideImageExportPresentation();
    SNOW_SHOT_PIN_PERF_MILESTONE("controller.presentation_hidden");
    if (shouldSnapshotHistory && !prepareHistoryCandidate(historyCandidate.get())) {
        static_cast<void>(finishImageExport(*exportGeneration));
        m_captureWorkflow->cancelCapture();
    }
}

void ScreenshotController::Impl::setScrollingScreenshotRecognitionMode(
    ScreenshotScrollingRecognitionMode mode) {
    if (m_scrollingCaptureController == nullptr || !m_scrollingCaptureController->active()) {
        return;
    }
    static_cast<void>(m_scrollingCaptureController->setRecognitionMode(mode));
}

void ScreenshotController::Impl::cancelCapture() {
    ++m_imageExportGeneration;
    m_imageExportInFlight = false;
    resetPendingCaptureRequest();
    m_ocrController->invalidateSession();
    static_cast<void>(stopScrollingCapture(false));
    if (m_historyService != nullptr) {
        m_historyService->resetCaptureNavigation();
    }
    m_captureWorkflow->cancelCapture();
}

void ScreenshotController::Impl::copySelectionToClipboard() {
    copySelectionToClipboardWithSource(
        snow_shot::storage::CaptureHistorySource::CopiedToClipboard);
}

void ScreenshotController::Impl::copySelectionToClipboardWithSource(
    snow_shot::storage::CaptureHistorySource historySource) {
    if (m_ocrController->active()) {
        if (m_ocrController->copyRecognitionToClipboard()) {
            return;
        }
        m_messages->error(QString::fromLatin1(kCopyMessageKey),
                          QCoreApplication::translate("ScreenshotController",
                                                      "No recognized result is available to copy"));
        return;
    }
    if (m_scrollingCaptureController != nullptr && m_scrollingCaptureController->active()) {
        const std::optional<quint64> exportGeneration = beginImageExport();
        if (!exportGeneration.has_value()) {
            return;
        }
        const QPointer<ScreenshotController> receiver(&owner);
        const bool scheduled = m_scrollingCaptureController->requestTrimmedClipboardPayload(
            [receiver, generation = *exportGeneration](ScreenshotClipboardPayload payload) {
                if (receiver.isNull() || receiver->m_impl == nullptr) {
                    return;
                }
                if (!receiver->m_impl->imageExportCurrent(generation)) {
                    return;
                }
                if (payload.isValid() && ScreenshotClipboardService::publish(
                                             QApplication::clipboard(), std::move(payload))) {
                    receiver->m_impl->completeScrollingResultExport(generation);
                    return;
                }
                qWarning("Scrolling screenshot clipboard export failed");
                receiver->m_impl->completeScrollingResultExport(generation);
            });
        if (!scheduled) {
            static_cast<void>(finishImageExport(*exportGeneration));
            qWarning("Failed to schedule scrolling screenshot clipboard export");
            return;
        }
        hideImageExportPresentation();
        return;
    }
    const bool historyEligible = m_interaction.activeTool() != ScreenshotActiveTool::Ocr &&
                                 m_interaction.activeTool() != ScreenshotActiveTool::Table &&
                                 m_interaction.activeTool() != ScreenshotActiveTool::Qr;
    m_ocrController->deactivate();
    const std::optional<quint64> exportGeneration = beginImageExport();
    if (!exportGeneration.has_value()) {
        return;
    }
    hideImageExportPresentation();
    const bool shouldSnapshotHistory =
        historyEligible && m_historyService != nullptr && resetCanvasEditingState();
    auto historyCandidate = std::make_shared<std::optional<ScreenshotHistoryEntry>>();
    const QPointer<ScreenshotController> receiver(&owner);
    const bool scheduled = m_selectionExportWorkflow->copySelectionToClipboard(
        [receiver, generation = *exportGeneration]() {
            return !receiver.isNull() && receiver->m_impl != nullptr &&
                   receiver->m_impl->imageExportCurrent(generation);
        },
        [receiver, generation = *exportGeneration, historyCandidate, historySource](bool success) mutable {
            if (receiver.isNull() || receiver->m_impl == nullptr ||
                !receiver->m_impl->finishImageExport(generation)) {
                return;
            }
            if (success && historyCandidate->has_value() &&
                receiver->m_impl->m_historyService != nullptr) {
                historyCandidate->value().source = historySource;
                receiver->m_impl->m_historyService->commit(
                    std::move(historyCandidate->value()));
            } else if (!success) {
                qWarning("Screenshot clipboard export failed");
            }
            if (receiver->m_impl->m_historyService != nullptr) {
                receiver->m_impl->m_historyService->resetCaptureNavigation();
            }
            receiver->m_impl->m_ocrController->invalidateSession();
            receiver->m_impl->m_captureWorkflow->cancelCapture();
        });
    if (!scheduled) {
        static_cast<void>(finishImageExport(*exportGeneration));
        qWarning("Failed to schedule screenshot clipboard export");
        m_captureWorkflow->cancelCapture();
        return;
    }
    if (shouldSnapshotHistory && !prepareHistoryCandidate(historyCandidate.get())) {
        static_cast<void>(finishImageExport(*exportGeneration));
        m_captureWorkflow->cancelCapture();
    }
}

bool ScreenshotController::Impl::resetCanvasEditingState() {
    return m_overlayCoordinator != nullptr &&
           m_overlayCoordinator->resetEditingState(m_displaySession);
}

bool ScreenshotController::Impl::prepareHistoryCandidate(
    std::optional<ScreenshotHistoryEntry>* candidate) {
    if (candidate == nullptr) {
        return true;
    }
    candidate->reset();
    if (m_historyService == nullptr) {
        return true;
    }
    *candidate = m_historyService->snapshotCurrent(true);
    return true;
}

void ScreenshotController::Impl::startVideoRecording() {
    m_ocrController->deactivate();
    if (!m_selection.hasPixelSelection() || m_videoRecordingController == nullptr ||
        (m_scrollingCaptureController != nullptr && m_scrollingCaptureController->active())) {
        return;
    }
    QRect physicalRegion = m_selection.pixelSelection().translated(m_geometry.canvasOrigin());
    if (physicalRegion.width() < 2 || physicalRegion.height() < 2) {
        return;
    }
    static_cast<void>(resetCanvasEditingState());

    m_ocrController->invalidateSession();
    m_captureWorkflow->cancelCapture();
    if (m_historyService != nullptr) {
        m_historyService->resetCaptureNavigation();
    }
    QTimer::singleShot(
        0, &owner, [this, physicalRegion]() { m_videoRecordingController->open(physicalRegion); });
}

void ScreenshotController::Impl::setShapeStyleFromToolbar(const SnowCanvasShapeStyle& style,
                                                          quint32 properties,
                                                          SnowCanvasShapeKind kind) {
    m_toolCommandWorkflow->setShapeStyleFromToolbar(style, properties, kind);
}

void ScreenshotController::Impl::setLineTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setLineTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setFreeDrawTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setFreeDrawTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setHighlightTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setHighlightTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setPenHighlightTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setPenHighlightTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setEraserTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setEraserTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setFilterTool() {
    setRectangleFilterTool();
}

void ScreenshotController::Impl::setSpotlightTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setSpotlightTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setRectangleFilterTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setRectangleFilterTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setPenFilterTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setPenFilterTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setWatermarkTool() {
    m_ocrController->deactivate();
    const bool scrollingCaptureStopped = stopScrollingCapture(true);
    m_toolCommandWorkflow->setWatermarkTool();
    restoreToolUiAfterScrollingCapture(scrollingCaptureStopped);
}

void ScreenshotController::Impl::setWatermarkConfigFromToolbar(
    const SnowCanvasWatermarkConfig& config) {
    m_toolCommandWorkflow->setWatermarkConfigFromToolbar(config);
}

void ScreenshotController::Impl::setSpotlightConfigFromToolbar(
    const SnowCanvasSpotlightConfig& config) {
    m_toolCommandWorkflow->setSpotlightConfigFromToolbar(config);
}

void ScreenshotController::Impl::previewSpotlightFromToolbar(
    const SnowCanvasSpotlightConfig& config) {
    m_overlayCoordinator->previewSpotlightConfig(m_displaySession, config);
}

void ScreenshotController::Impl::previewWatermarkFromToolbar(
    const SnowCanvasWatermarkConfig& config) {
    m_overlayCoordinator->previewWatermarkConfig(m_displaySession, config);
}

void ScreenshotController::Impl::setFilterStyleFromToolbar(const SnowCanvasFilterStyle& style,
                                                           quint32 properties) {
    m_toolCommandWorkflow->setFilterStyleFromToolbar(style, properties);
}

void ScreenshotController::Impl::setTextStyleFromToolbar(const SnowCanvasTextStyle& style) {
    m_toolCommandWorkflow->setTextStyleFromToolbar(style);
}

void ScreenshotController::Impl::setSerialNumberStyleFromToolbar(
    const SnowCanvasSerialNumberStyle& style) {
    m_toolCommandWorkflow->setSerialNumberStyleFromToolbar(style);
}

void ScreenshotController::Impl::decrementSelectedSerialNumbers() {
    m_toolCommandWorkflow->decrementSelectedSerialNumbers();
}

void ScreenshotController::Impl::incrementSelectedSerialNumbers() {
    m_toolCommandWorkflow->incrementSelectedSerialNumbers();
}

void ScreenshotController::Impl::createTextForSelectedSerialNumber() {
    m_toolCommandWorkflow->createTextForSelectedSerialNumber();
}

void ScreenshotController::Impl::repositionToolbarForContentChange() {
    m_selectionEditWorkflow->repositionToolbarForContentChange();
}

void ScreenshotController::Impl::toggleSelectionAspectRatioLockFromToolbar() {
    m_selectionEditWorkflow->toggleSelectionAspectRatioLockFromToolbar();
}

void ScreenshotController::Impl::openSelectionResizeModalFromToolbar() {
    m_selectionEditWorkflow->openSelectionResizeModalFromToolbar();
}

void ScreenshotController::Impl::hideColorPickersForScreenshotUi() {
    m_selectionEditWorkflow->hideColorPickersForScreenshotUi();
}

void ScreenshotController::Impl::adjustSelectionFromToolbar(int minDx, int minDy, int maxDx,
                                                            int maxDy) {
    m_selectionEditWorkflow->adjustSelectionFromToolbar(minDx, minDy, maxDx, maxDy);
}

void ScreenshotController::Impl::setSelectionCornerRadiusFromToolbar(int radius) {
    m_selectionEditWorkflow->setSelectionCornerRadiusFromToolbar(radius);
}

void ScreenshotController::Impl::setSelectionShadowWidthFromToolbar(int shadowWidth) {
    m_selectionEditWorkflow->setSelectionShadowWidthFromToolbar(shadowWidth);
}

ScreenshotController::Impl::~Impl() {
    shutdown();
}

void ScreenshotController::Impl::invalidateDelayedCapture() {
    ++m_delayedCaptureGeneration;
}

void ScreenshotController::Impl::resetPendingCaptureRequest() {
    invalidateDelayedCapture();
    m_pendingSelectionAction = PendingSelectionAction::None;
    m_pendingHistorySource = snow_shot::storage::CaptureHistorySource::CopiedToClipboard;
    m_automaticSelectionMode = AutomaticSelectionMode::None;
    m_automaticPhysicalPoint = QPoint();
    m_focusedWindowCapture.reset();
}

bool ScreenshotController::Impl::canBeginCapture() const {
    if (m_imageExportInFlight || m_captureState.captureInProgress || !m_interaction.inactive() ||
        (m_captureState.sessionState != ScreenshotSessionState::IdleCold &&
         m_captureState.sessionState != ScreenshotSessionState::IdlePrepared)) {
        return false;
    }
    return m_captureWorkflow != nullptr && m_ocrController != nullptr;
}

bool ScreenshotController::Impl::beginCapture(
    PendingSelectionAction action, snow_shot::storage::CaptureHistorySource historySource,
    AutomaticSelectionMode automaticMode, const QPoint& automaticPhysicalPoint) {
    if (!canBeginCapture()) {
        return false;
    }

    invalidateDelayedCapture();
    ++m_imageExportGeneration;
    m_imageExportInFlight = false;
    m_pendingHistoryEditRecordId.clear();
    m_pendingSelectionAction = action;
    m_pendingHistorySource = historySource;
    m_automaticSelectionMode = automaticMode;
    m_automaticPhysicalPoint = automaticPhysicalPoint;
    if (automaticMode != AutomaticSelectionMode::FocusedWindow) {
        m_focusedWindowCapture.reset();
    }
    m_ocrController->invalidateSession();
    static_cast<void>(stopScrollingCapture(false));
    if (m_historyService != nullptr) {
        m_historyService->resetCaptureNavigation();
    }
    m_captureWorkflow->startCapture();
    return true;
}

void ScreenshotController::Impl::handleSelectionConfirmed() {
    const PendingSelectionAction action =
        std::exchange(m_pendingSelectionAction, PendingSelectionAction::None);
    const snow_shot::storage::CaptureHistorySource source = m_pendingHistorySource;
    m_pendingHistorySource = snow_shot::storage::CaptureHistorySource::CopiedToClipboard;
    if (action == PendingSelectionAction::None) {
        return;
    }

    const quint64 sessionId = m_captureState.sessionId;
    QTimer::singleShot(0, &owner, [this, action, source, sessionId]() {
        if (m_captureState.sessionId != sessionId ||
            m_captureState.sessionState != ScreenshotSessionState::Editing) {
            return;
        }
        switch (action) {
        case PendingSelectionAction::Pin:
            pinSelectionToScreen();
            break;
        case PendingSelectionAction::RecognizeText:
            setOcrTool();
            break;
        case PendingSelectionAction::Copy:
            copySelectionToClipboardWithSource(source);
            break;
        case PendingSelectionAction::StartVideo:
            startVideoRecording();
            break;
        case PendingSelectionAction::None:
            break;
        }
    });
}

void ScreenshotController::Impl::applyFocusedWindowCapture() {
    if (!m_focusedWindowCapture.has_value() ||
        !m_focusedWindowCapture->isValid()) {
        return;
    }

    const ScreenshotWindowCaptureFrame& frame = *m_focusedWindowCapture;
    m_displaySession.forEachMutableActiveDisplay([&frame](qsizetype, CapturedDisplayModel& display) {
        const QRect intersection = display.physicalRect.intersected(frame.physicalRect);
        if (intersection.isEmpty() || display.image.isNull()) {
            return;
        }

        const QRect displayRect = display.physicalRect;
        const QRect frameRect = frame.physicalRect;
        const auto mapRect = [](const QRect& sourceRect, const QRect& referenceRect,
                                const QSize& imageSize) {
            if (sourceRect.isEmpty() || referenceRect.isEmpty() || imageSize.isEmpty()) {
                return QRect();
            }

            const qreal sx =
                static_cast<qreal>(imageSize.width()) / referenceRect.width();
            const qreal sy =
                static_cast<qreal>(imageSize.height()) / referenceRect.height();
            const qreal sourceRight =
                static_cast<qreal>(sourceRect.left()) + sourceRect.width();
            const qreal sourceBottom =
                static_cast<qreal>(sourceRect.top()) + sourceRect.height();
            const int left = qFloor((static_cast<qreal>(sourceRect.left()) -
                                     static_cast<qreal>(referenceRect.left())) *
                                    sx);
            const int top = qFloor((static_cast<qreal>(sourceRect.top()) -
                                    static_cast<qreal>(referenceRect.top())) *
                                   sy);
            const int right =
                qCeil((sourceRight - static_cast<qreal>(referenceRect.left())) * sx);
            const int bottom =
                qCeil((sourceBottom - static_cast<qreal>(referenceRect.top())) * sy);
            return QRect(left, top, std::max(0, right - left), std::max(0, bottom - top))
                .intersected(QRect(QPoint(), imageSize));
        };
        const QRect source = mapRect(intersection, frameRect, frame.image.size());
        const QRect target = mapRect(intersection, displayRect, display.image.size());
        if (source.isEmpty() || target.isEmpty()) {
            return;
        }
        QPainter painter(&display.image);
        painter.drawImage(target, frame.image, source);
    });
    if (m_overlayCoordinator != nullptr) {
        m_overlayCoordinator->applyDisplayModels(m_displaySession);
    }
}

void ScreenshotController::Impl::executeAutomaticSelection() {
    const AutomaticSelectionMode mode =
        std::exchange(m_automaticSelectionMode, AutomaticSelectionMode::None);
    if (mode == AutomaticSelectionMode::None || m_overlayInputHandler == nullptr) {
        return;
    }

    if (mode == AutomaticSelectionMode::FocusedWindow) {
        applyFocusedWindowCapture();
    }

    const CapturedDisplayModel* display = nullptr;
    if (mode == AutomaticSelectionMode::CurrentMonitor) {
        display = m_geometry.displayForPhysicalPoint(m_displaySession,
                                                     m_automaticPhysicalPoint);
    }

    QRectF selection;
    if (mode == AutomaticSelectionMode::FocusedWindow && m_focusedWindowCapture.has_value()) {
        selection = m_geometry.canvasRectForPhysicalRect(
            m_displaySession, m_focusedWindowCapture->physicalRect);
    } else if (display != nullptr) {
        selection = display->canvasRect;
    }
    if (!selection.isValid() || selection.isEmpty()) {
        m_automaticSelectionMode = AutomaticSelectionMode::None;
        m_focusedWindowCapture.reset();
        cancelCapture();
        return;
    }
    m_selection.setSelectionRect(selection);
    m_automaticSelectionMode = AutomaticSelectionMode::None;
    m_focusedWindowCapture.reset();
    m_overlayInputHandler->confirmSelection();
}

void ScreenshotController::Impl::shutdown() {
    ++m_imageExportGeneration;
    m_imageExportInFlight = false;
    resetPendingCaptureRequest();
    m_exportService.reset();
    m_ocrController.reset();
    static_cast<void>(stopScrollingCapture(false));
    if (m_captureWorkflow != nullptr) {
        m_captureWorkflow->cancelCapture();
        m_captureWorkflow->destroyDisplayPool();
        m_captureWorkflow->shutdownCaptureWorker();
        m_captureWorkflow->destroyUiSelectorService();
    }
    if (m_historyService != nullptr) {
        m_historyService->resetCaptureNavigation();
        m_historyService->drainPendingWrites();
    }
    if (m_selectorCoordinator != nullptr) {
        QObject::disconnect(m_selectorCoordinator, nullptr, &owner, nullptr);
    }
    if (m_overlayEventAdapter != nullptr) {
        m_overlayEventAdapter->clearEventTargets();
    }
    m_overlayInputHandler.reset();
    m_scrollingCaptureController.reset();
    m_displayConfigurationObserver.reset();
    m_historyService.reset();
    m_captureWorkflow.reset();
    m_captureRuntime.reset();
    m_selectionEditWorkflow.reset();
    m_toolCommandWorkflow.reset();
    m_selectorWorkflow.reset();
    m_presentationServices.reset();
    m_colorPickerController.reset();
    m_toolbarPresenter.reset();
    m_selectionResizeWorkflow.reset();
    m_selectionExportWorkflow.reset();
    m_selectionExportUiServices.reset();
    m_selectionSettings.reset();
    m_videoRecordingController.reset();
    m_overlayCoordinator.reset();
    m_overlayEventAdapter.reset();
}

ScreenshotController::ScreenshotController(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this)) {}

ScreenshotController::~ScreenshotController() = default;

void ScreenshotController::prewarmResources() {
    QTimer::singleShot(0, this, [this]() { m_impl->m_captureWorkflow->prewarmResources(); });
}

void ScreenshotController::startCapture() {
    static_cast<void>(m_impl->beginCapture());
}

void ScreenshotController::startDelayedCapture(int delaySeconds) {
    if (!m_impl->canBeginCapture()) {
        return;
    }
    const int seconds = std::clamp(delaySeconds, 1, 10);
    const quint64 generation = ++m_impl->m_delayedCaptureGeneration;
    QTimer::singleShot(seconds * 1000, this, [this, generation]() {
        if (m_impl == nullptr || generation != m_impl->m_delayedCaptureGeneration ||
            !m_impl->canBeginCapture()) {
            return;
        }
        startCapture();
    });
}

void ScreenshotController::captureAndPinSelection() {
    static_cast<void>(m_impl->beginCapture(
        Impl::PendingSelectionAction::Pin));
}

void ScreenshotController::captureAndRecognizeText() {
    static_cast<void>(m_impl->beginCapture(
        Impl::PendingSelectionAction::RecognizeText));
}

void ScreenshotController::captureAndCopySelection() {
    static_cast<void>(m_impl->beginCapture(
        Impl::PendingSelectionAction::Copy));
}

void ScreenshotController::captureCurrentMonitor() {
    if (!m_impl->canBeginCapture()) {
        return;
    }
#if defined(Q_OS_WIN) || defined(_WIN32)
    POINT nativeCursor{};
    if (GetCursorPos(&nativeCursor) == FALSE) {
        qWarning("Failed to query the physical cursor position for current-monitor capture");
        return;
    }
    const QPoint cursorPosition(nativeCursor.x, nativeCursor.y);
#else
    const QPoint cursorPosition = QCursor::pos();
#endif
    static_cast<void>(m_impl->beginCapture(
        Impl::PendingSelectionAction::Copy,
        snow_shot::storage::CaptureHistorySource::CurrentMonitor,
        Impl::AutomaticSelectionMode::CurrentMonitor, cursorPosition));
}

void ScreenshotController::captureFocusedWindow() {
#if defined(Q_OS_WIN) || defined(_WIN32)
    if (!m_impl->canBeginCapture()) {
        return;
    }
    const HWND foreground = GetForegroundWindow();
    if (foreground == nullptr) {
        return;
    }
    ScreenshotWindowCapture capture(reinterpret_cast<quintptr>(foreground));
    const auto frame = capture.capture();
    if (!frame.has_value() || !frame->isValid()) {
        qWarning("Focused window capture failed: %s",
                 qPrintable(capture.errorMessage()));
        return;
    }
    m_impl->m_focusedWindowCapture = *frame;
    if (!m_impl->beginCapture(
            Impl::PendingSelectionAction::Copy,
            snow_shot::storage::CaptureHistorySource::FocusedWindow,
            Impl::AutomaticSelectionMode::FocusedWindow)) {
        m_impl->m_focusedWindowCapture.reset();
    }
#else
    Q_UNUSED(this);
#endif
}

void ScreenshotController::captureAndStartVideoRecording() {
    static_cast<void>(m_impl->beginCapture(
        Impl::PendingSelectionAction::StartVideo));
}

void ScreenshotController::startOrStopVideoRecordingAndCopy() {
    if (m_impl->m_videoRecordingController == nullptr) {
        return;
    }
    if (!m_impl->m_videoRecordingController->isOpen()) {
        captureAndStartVideoRecording();
    } else if (!m_impl->m_videoRecordingController->isRecording()) {
        m_impl->m_videoRecordingController->startRecording();
    } else {
        m_impl->m_videoRecordingController->stopRecordingAndCopyVideo();
    }
}

void ScreenshotController::editHistoryRecord(const QString& recordId) {
    ++m_impl->m_imageExportGeneration;
    m_impl->m_imageExportInFlight = false;
    m_impl->startHistoryEdit(recordId);
}

void ScreenshotController::cancelCapture() {
    m_impl->cancelCapture();
}

void ScreenshotController::copySelectionToClipboard() {
    m_impl->copySelectionToClipboard();
}

void ScreenshotController::pinSelectionToScreen() {
    m_impl->pinSelectionToScreen();
}

void ScreenshotController::startVideoRecording() {
    m_impl->startVideoRecording();
}

void ScreenshotController::setMoveTool() {
    m_impl->setMoveTool();
}

void ScreenshotController::setSelectTool() {
    m_impl->setSelectTool();
}

void ScreenshotController::setShapeTool() {
    m_impl->setShapeTool();
}

void ScreenshotController::setArrowTool() {
    m_impl->setArrowTool();
}

void ScreenshotController::setLineTool() {
    m_impl->setLineTool();
}

void ScreenshotController::setFreeDrawTool() {
    m_impl->setFreeDrawTool();
}

void ScreenshotController::setHighlightTool() {
    m_impl->setHighlightTool();
}

void ScreenshotController::setPenHighlightTool() {
    m_impl->setPenHighlightTool();
}

void ScreenshotController::Impl::setSelectionToolbarHovered(bool hovered) {
    if (m_presentationServices != nullptr) {
        m_presentationServices->setSelectionToolbarHovered(hovered);
    }
}

void ScreenshotController::setSpotlightTool() {
    m_impl->setSpotlightTool();
}

void ScreenshotController::setEraserTool() {
    m_impl->setEraserTool();
}

void ScreenshotController::setFilterTool() {
    m_impl->setFilterTool();
}

void ScreenshotController::setRectangleFilterTool() {
    m_impl->setRectangleFilterTool();
}

void ScreenshotController::setPenFilterTool() {
    m_impl->setPenFilterTool();
}

void ScreenshotController::setWatermarkTool() {
    m_impl->setWatermarkTool();
}

void ScreenshotController::setWatermarkConfigFromToolbar(const SnowCanvasWatermarkConfig& config) {
    m_impl->setWatermarkConfigFromToolbar(config);
}

void ScreenshotController::setSpotlightConfigFromToolbar(const SnowCanvasSpotlightConfig& config) {
    m_impl->setSpotlightConfigFromToolbar(config);
}

void ScreenshotController::setTextTool() {
    m_impl->setTextTool();
}

void ScreenshotController::setSerialNumberTool() {
    m_impl->setSerialNumberTool();
}

void ScreenshotController::decrementSelectedSerialNumbers() {
    m_impl->decrementSelectedSerialNumbers();
}

void ScreenshotController::incrementSelectedSerialNumbers() {
    m_impl->incrementSelectedSerialNumbers();
}

void ScreenshotController::createTextForSelectedSerialNumber() {
    m_impl->createTextForSelectedSerialNumber();
}

SnowCanvasShapeStyle ScreenshotController::currentRectangleStyle() const {
    return m_impl->m_toolCommandWorkflow->currentRectangleStyle();
}

void ScreenshotController::setShapeStyleFromToolbar(const SnowCanvasShapeStyle& style,
                                                    quint32 properties, SnowCanvasShapeKind kind) {
    m_impl->setShapeStyleFromToolbar(style, properties, kind);
}

void ScreenshotController::Impl::reorderSelectedElements(SnowCanvasSelectionOrder order) {
    m_overlayCoordinator->reorderSelectedElements(m_displaySession, order);
}

void ScreenshotController::Impl::setSelectedElementsOpacity(qreal opacity) {
    m_overlayCoordinator->setSelectedElementsOpacity(m_displaySession, opacity);
}

void ScreenshotController::Impl::duplicateSelectedElements() {
    m_overlayCoordinator->duplicateSelectedElements(m_displaySession);
}

void ScreenshotController::Impl::deleteSelectedElements() {
    m_overlayCoordinator->deleteSelectedElements(m_displaySession);
}

void ScreenshotController::setTextStyleFromToolbar(const SnowCanvasTextStyle& style) {
    m_impl->setTextStyleFromToolbar(style);
}

void ScreenshotController::setSerialNumberStyleFromToolbar(
    const SnowCanvasSerialNumberStyle& style) {
    m_impl->setSerialNumberStyleFromToolbar(style);
}

void ScreenshotController::adjustSelectionFromToolbar(int minDx, int minDy, int maxDx, int maxDy) {
    m_impl->adjustSelectionFromToolbar(minDx, minDy, maxDx, maxDy);
}

void ScreenshotController::setSelectionCornerRadiusFromToolbar(int radius) {
    m_impl->setSelectionCornerRadiusFromToolbar(radius);
}

void ScreenshotController::setSelectionShadowWidthFromToolbar(int shadowWidth) {
    m_impl->setSelectionShadowWidthFromToolbar(shadowWidth);
}

void ScreenshotController::toggleSelectionAspectRatioLockFromToolbar() {
    m_impl->toggleSelectionAspectRatioLockFromToolbar();
}

void ScreenshotController::openSelectionResizeModalFromToolbar() {
    m_impl->openSelectionResizeModalFromToolbar();
}

void ScreenshotController::repositionToolbarForContentChange() {
    m_impl->repositionToolbarForContentChange();
}

void ScreenshotController::hideColorPickersForScreenshotUi() {
    m_impl->hideColorPickersForScreenshotUi();
}
