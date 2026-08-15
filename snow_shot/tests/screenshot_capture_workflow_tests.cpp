#include "snow_shot/presentation/screenshotcapturestate.h"
#include "snow_shot/presentation/screenshotcapturedisplaymodelreconciler.h"
#include "snow_shot/presentation/screenshotcaptureworkflow.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotintelligentselectionmodel.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

class CaptureRuntime final : public ScreenshotCaptureRuntimePort {
  public:
    void setEventSink(ScreenshotCaptureWorkerEventSink* sink) override {
        eventSink = sink;
    }

    [[nodiscard]] bool captureWorkerCreated() const override {
        return true;
    }
    [[nodiscard]] bool hasCaptureWorker() const override {
        return true;
    }
    void ensureCaptureWorker() override {}
    void prepareAsync(quint64) override {
        ++prepareAsyncCalls;
    }
    void captureAllAsync(quint64, bool) override {
        ++captureAllAsyncCalls;
        selectorRefreshWasInFlightAtCapture = selectorRefreshActive;
    }
    void releaseIdleResourcesAsync(quint64) override {}
    void shutdownCaptureWorker() override {}

    [[nodiscard]] bool selectorReady() const override {
        return selectorIsReady;
    }
    [[nodiscard]] bool selectorRefreshInFlight() const override {
        return selectorRefreshActive;
    }
    [[nodiscard]] bool selectorHitTestInFlight() const override {
        return false;
    }
    void releaseSelectorCache() override {
        ++releaseSelectorCacheCalls;
        selectorIsReady = false;
        selectorRefreshActive = false;
    }
    void resetHitTestState() override {}
    void destroySelectorService() override {}
    void startWorkflowRefresh() override {
        ++startWorkflowRefreshCalls;
        selectorIsReady = false;
        selectorRefreshActive = true;
    }
    void clearSelectorSelection() override {}
    [[nodiscard]] bool updateSelectorSelectionAt(const QPoint&) override {
        return false;
    }

    void prewarmDisplayPool(ScreenshotDisplaySession&, int) override {}
    void ensureToolbar() override {}
    void prewarmToolbar() override {
        ++prewarmToolbarCalls;
    }
    void clearOverlayCanvases(const ScreenshotDisplaySession&) const override {
        ++clearOverlayCanvasCalls;
    }
    void clearDisplays(ScreenshotDisplaySession&) override {
        ++clearDisplayCalls;
    }
    void destroyDisplayPool(ScreenshotDisplaySession&) override {}
    void resetForNewCapture(ScreenshotDisplaySession&) override {}
    void prepareDisplayModels(ScreenshotDisplaySession&) override {}
    void applyDisplayModels(ScreenshotDisplaySession&) override {
        ++applyDisplayModelsCalls;
    }
    [[nodiscard]] bool preparePreCaptureOverlayWindows(ScreenshotDisplaySession&) override {
        ++preparePreCaptureOverlayCalls;
        return true;
    }
    void showOverlayWindows(const ScreenshotDisplaySession&, ScreenshotOverlayShowMode) override {
        ++showOverlayCalls;
    }
    void hideOverlayWindows(const ScreenshotDisplaySession&) override {
        ++hideOverlayCalls;
    }

    [[nodiscard]] bool clearDocumentPreservingViewports() override {
        ++clearDocumentCalls;
        return true;
    }
    [[nodiscard]] bool resetCanvasRuntime() override {
        return true;
    }
    void resetColorPicker() override {}

    ScreenshotCaptureWorkerEventSink* eventSink = nullptr;
    int prepareAsyncCalls = 0;
    int captureAllAsyncCalls = 0;
    int startWorkflowRefreshCalls = 0;
    int releaseSelectorCacheCalls = 0;
    mutable int clearOverlayCanvasCalls = 0;
    int clearDisplayCalls = 0;
    int showOverlayCalls = 0;
    int applyDisplayModelsCalls = 0;
    int preparePreCaptureOverlayCalls = 0;
    int hideOverlayCalls = 0;
    int clearDocumentCalls = 0;
    int prewarmToolbarCalls = 0;
    bool selectorIsReady = false;
    bool selectorRefreshActive = false;
    bool selectorRefreshWasInFlightAtCapture = false;
};

ScreenshotCaptureWorkflow
makeWorkflow(ScreenshotCaptureState& state, ScreenshotDisplaySession& displaySession,
             ScreenshotGeometryMapper& geometry, ScreenshotInteractionState& interaction,
             ScreenshotSelectionModel& selection,
             ScreenshotIntelligentSelectionModel& intelligentSelection, CaptureRuntime& runtime) {
    return ScreenshotCaptureWorkflow({
        state,
        runtime,
        geometry,
        displaySession,
        interaction,
        selection,
        intelligentSelection,
        {},
    });
}

void idlePrewarmDoesNotInitializeSelector() {
    ScreenshotCaptureState state;
    ScreenshotDisplaySession displaySession;
    ScreenshotGeometryMapper geometry;
    ScreenshotInteractionState interaction;
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligentSelection;
    CaptureRuntime runtime;
    auto workflow = makeWorkflow(state, displaySession, geometry, interaction, selection,
                                 intelligentSelection, runtime);

    workflow.prewarmResources();
    workflow.prewarmResources();
    require(runtime.prewarmToolbarCalls == 1,
            "idle toolbar prewarm must be idempotent once resources are prepared");
    require(state.sessionState == ScreenshotSessionState::IdlePrepared,
            "idle prewarm must leave the workflow prepared");
    require(runtime.startWorkflowRefreshCalls == 0 && !runtime.selectorRefreshActive,
            "idle prewarm must not initialize the selector cache");

    workflow.startCapture();
    workflow.prewarmResources();
    require(runtime.prewarmToolbarCalls == 1, "active capture must not run idle toolbar prewarm");
    require(runtime.startWorkflowRefreshCalls == 1 && runtime.selectorRefreshActive,
            "capture start must initialize the selector cache");
}

void cancelClearsTheReusableCanvasDocument() {
    ScreenshotCaptureState state;
    state.sessionState = ScreenshotSessionState::Editing;
    state.captureInProgress = true;
    ScreenshotDisplaySession displaySession;
    ScreenshotGeometryMapper geometry;
    ScreenshotInteractionState interaction;
    interaction.beginCapture();
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligentSelection;
    CaptureRuntime runtime;
    int captureTerminatedCalls = 0;

    ScreenshotCaptureWorkflow workflow({
        state,
        runtime,
        geometry,
        displaySession,
        interaction,
        selection,
        intelligentSelection,
        {},
        [&captureTerminatedCalls]() { ++captureTerminatedCalls; },
    });

    workflow.cancelCapture();

    require(runtime.clearDocumentCalls == 1,
            "canceling a capture must clear the reusable canvas document");
    require(runtime.clearOverlayCanvasCalls == 1,
            "clearing the canceled document must refresh reused overlay canvases");
    require(runtime.hideOverlayCalls == 1 && runtime.clearDisplayCalls == 1,
            "canceling a capture must still release its visible display session");
    require(runtime.releaseSelectorCacheCalls == 1,
            "canceling a capture must immediately release the selector cache");
    require(captureTerminatedCalls == 1,
            "canceling a capture must stop active capture-scoped features before cleanup");
    require(state.sessionState == ScreenshotSessionState::IdlePrepared,
            "canceling a capture must return the workflow to its prepared idle state");
}

void captureInitializesSelectorBeforeCapturing() {
    const auto runScenario = [](bool selectorReady, bool selectorRefreshActive) {
        ScreenshotCaptureState state;
        state.sessionState = ScreenshotSessionState::IdlePrepared;
        ScreenshotDisplaySession displaySession;
        ScreenshotGeometryMapper geometry;
        ScreenshotInteractionState interaction;
        ScreenshotSelectionModel selection;
        ScreenshotIntelligentSelectionModel intelligentSelection;
        CaptureRuntime runtime;
        runtime.selectorIsReady = selectorReady;
        runtime.selectorRefreshActive = selectorRefreshActive;

        auto workflow = makeWorkflow(state, displaySession, geometry, interaction, selection,
                                     intelligentSelection, runtime);

        workflow.startCapture();

        require(runtime.startWorkflowRefreshCalls == 1,
                "capture must initialize the selector snapshot");
        require(runtime.captureAllAsyncCalls == 1 && runtime.selectorRefreshWasInFlightAtCapture,
                "the capture-session selector refresh must start before desktop capture");
    };

    runScenario(false, false);
}

void restartingCaptureReleasesPreviousSelectorCache() {
    ScreenshotCaptureState state;
    state.sessionState = ScreenshotSessionState::Editing;
    state.captureInProgress = true;
    ScreenshotDisplaySession displaySession;
    ScreenshotGeometryMapper geometry;
    ScreenshotInteractionState interaction;
    interaction.beginCapture();
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligentSelection;
    CaptureRuntime runtime;
    runtime.selectorIsReady = true;
    int captureTerminatedCalls = 0;
    ScreenshotCaptureWorkflow workflow({
        state,
        runtime,
        geometry,
        displaySession,
        interaction,
        selection,
        intelligentSelection,
        {},
        [&captureTerminatedCalls]() { ++captureTerminatedCalls; },
    });

    workflow.startCapture();

    require(runtime.releaseSelectorCacheCalls == 1,
            "starting a new capture must release the previous selector cache");
    require(runtime.startWorkflowRefreshCalls == 1,
            "the restarted capture must initialize a fresh selector snapshot");
    require(captureTerminatedCalls == 1,
            "restarting a capture must stop features owned by the previous capture");
}

void capturePresentedRunsAfterCapturedOverlayIsShown() {
    ScreenshotCaptureState state;
    state.sessionState = ScreenshotSessionState::IdlePrepared;
    ScreenshotDisplaySession displaySession;
    ScreenshotGeometryMapper geometry;
    ScreenshotInteractionState interaction;
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligentSelection;
    CaptureRuntime runtime;
    int capturePresentedCalls = 0;
    int showCallsObservedByCallback = 0;
    ScreenshotCaptureWorkflow workflow({
        state,
        runtime,
        geometry,
        displaySession,
        interaction,
        selection,
        intelligentSelection,
        ScreenshotCapturePresentationCallbacks{
            {},
            {},
            {},
            [&capturePresentedCalls, &showCallsObservedByCallback, &runtime]() {
                ++capturePresentedCalls;
                showCallsObservedByCallback = runtime.showOverlayCalls;
            },
        },
    });

    CapturedDisplayModel snapshot;
    snapshot.stableId = QStringLiteral("primary");
    snapshot.name = QStringLiteral("Primary");
    snapshot.physicalRect = QRect(0, 0, 64, 48);
    snapshot.logicalRect = snapshot.physicalRect;
    snapshot.image = QImage(snapshot.physicalRect.size(), QImage::Format_RGBA8888);
    snapshot.image.fill(Qt::blue);

    workflow.startCapture();
    require(runtime.eventSink != nullptr, "capture workflow did not register its event sink");
    runtime.eventSink->handleCaptureFinished(state.sessionId, {snapshot});
    runtime.eventSink->handleCaptureFinished(state.sessionId, {snapshot});

    require(runtime.showOverlayCalls == 1 && capturePresentedCalls == 1 &&
                showCallsObservedByCallback == 1,
            "capture-presented callback must run once after the captured overlay is shown");
}

void silentCaptureNeverPreparesOrShowsOverlays() {
    ScreenshotCaptureState state;
    state.sessionState = ScreenshotSessionState::IdlePrepared;
    ScreenshotDisplaySession displaySession;
    ScreenshotGeometryMapper geometry;
    ScreenshotInteractionState interaction;
    ScreenshotSelectionModel selection;
    ScreenshotIntelligentSelectionModel intelligentSelection;
    CaptureRuntime runtime;
    int capturePresentedCalls = 0;
    ScreenshotCaptureWorkflow workflow({
        state,
        runtime,
        geometry,
        displaySession,
        interaction,
        selection,
        intelligentSelection,
        ScreenshotCapturePresentationCallbacks{
            {},
            {},
            {},
            [&capturePresentedCalls]() { ++capturePresentedCalls; },
        },
    });

    CapturedDisplayModel snapshot;
    snapshot.stableId = QStringLiteral("primary");
    snapshot.name = QStringLiteral("Primary");
    snapshot.physicalRect = QRect(0, 0, 64, 48);
    snapshot.image = QImage(snapshot.physicalRect.size(), QImage::Format_RGBA8888);
    snapshot.image.fill(Qt::blue);

    workflow.startCapture(ScreenshotCapturePresentationMode::Silent);
    require(runtime.eventSink != nullptr, "capture workflow did not register its event sink");
    runtime.eventSink->handleCaptureFinished(state.sessionId, {snapshot});

    require(runtime.preparePreCaptureOverlayCalls == 0,
            "silent capture must not prepare screenshot windows");
    require(runtime.showOverlayCalls == 0 && runtime.applyDisplayModelsCalls == 0,
            "silent capture must not bind or show screenshot windows");
    require(runtime.startWorkflowRefreshCalls == 0,
            "silent capture must not initialize smart selection");
    require(capturePresentedCalls == 1,
            "silent capture must notify the controller when pixels are ready");
}

void capturedImagePlacementFollowsNormalizedCanvasGeometry() {
    CapturedDisplayModel snapshot;
    snapshot.stableId = QStringLiteral("secondary-display");
    snapshot.name = QStringLiteral("Secondary");
    snapshot.physicalRect = QRect(1920, 240, 320, 180);
    snapshot.image = QImage(snapshot.physicalRect.size(), QImage::Format_RGBA8888);
    snapshot.image.fill(Qt::red);

    ScreenshotDisplaySession displaySession;
    ScreenshotCaptureDisplayModelReconciler::applySnapshots(displaySession, {snapshot});

    ScreenshotGeometryMapper geometry;
    geometry.rebuild(displaySession);

    const CapturedDisplayModel& display = displaySession.displayAt(0);
    require(ScreenshotGeometryMapper::displayCanvasRect(display) == QRectF(0, 0, 320, 180),
            "capture geometry must normalize a non-zero physical monitor origin");
    require(ScreenshotGeometryMapper::displayImageSourceCanvasRect(display) ==
                ScreenshotGeometryMapper::displayCanvasRect(display),
            "captured image placement must follow normalized canvas geometry");
}

void intelligentSelectionTargetsPreserveElementPathBehavior() {
    ScreenshotIntelligentSelectionModel selection;
    const QRectF nestedElement(30, 30, 20, 10);
    const QRectF childElement(20, 20, 60, 40);
    const QRectF window(10, 10, 100, 80);
    const QRectF bounds(0, 0, 200, 160);

    require(selection.selectionTarget() == ScreenshotIntelligentSelectionTarget::Window &&
                selection.applyCanvasHitPath({nestedElement, childElement, window}, bounds, 1.0) &&
                selection.index() == 2 && selection.currentSelection() == window,
            "window selection must use the outermost rectangle from an in-flight element path");
    require(selection.setIndex(0) && selection.index() == 2 &&
                selection.currentSelection() == window,
            "window selection must remain locked to the window rectangle");

    selection.toggleSelectionTarget();
    require(selection.selectionTarget() == ScreenshotIntelligentSelectionTarget::WindowSubElement &&
                selection.index() == 0 && selection.currentSelection() == nestedElement,
            "element selection must start at the deepest hit just as it did before target modes");
    require(selection.setIndex(1) && selection.currentSelection() == childElement &&
                selection.applyCanvasHitPath({nestedElement, childElement, window}, bounds, 1.0) &&
                selection.index() == 1,
            "an unchanged element hit path must preserve its selected level");

    const QRectF nextElement(130, 30, 20, 10);
    const QRectF nextWindow(120, 10, 70, 80);
    require(selection.applyCanvasHitPath({nextElement, nextWindow}, bounds, 1.0) &&
                selection.index() == 0 && selection.currentSelection() == nextElement,
            "a changed element hit path must restart at its deepest hit");
    require(selection.setIndex(99) && selection.index() == 1 &&
                selection.currentSelection() == nextWindow,
            "element selection must retain the original full-path navigation");
    require(selection.applyCanvasHitPath({nextWindow}, bounds, 1.0) && selection.index() == 0 &&
                selection.currentSelection() == nextWindow,
            "element selection must retain the original window fallback");
}
} // namespace

int main() {
    idlePrewarmDoesNotInitializeSelector();
    cancelClearsTheReusableCanvasDocument();
    captureInitializesSelectorBeforeCapturing();
    restartingCaptureReleasesPreviousSelectorCache();
    capturePresentedRunsAfterCapturedOverlayIsShown();
    silentCaptureNeverPreparesOrShowsOverlays();
    capturedImagePlacementFollowsNormalizedCanvasGeometry();
    intelligentSelectionTargetsPreserveElementPathBehavior();
    return 0;
}
