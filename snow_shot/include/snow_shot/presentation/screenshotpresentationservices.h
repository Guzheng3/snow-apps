#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTPRESENTATIONSERVICES_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTPRESENTATIONSERVICES_H

#include "snow_shot/presentation/screenshotsmartselectiontransition.h"
#include "snow_shot/presentation/screenshotuipreferences.h"

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>

struct ScreenshotCaptureState;
struct ScreenshotColorPickerContext;
class ScreenshotDisplaySession;
class ScreenshotGeometryMapper;
class ScreenshotInteractionState;
class ScreenshotOverlayCoordinator;
class ScreenshotSelectionModel;
class ScreenshotToolbarPresenter;
struct ScreenshotToolbarPresentationState;

struct ScreenshotPresentationServicesContext {
    ScreenshotCaptureState& captureState;
    ScreenshotOverlayCoordinator& overlayCoordinator;
    ScreenshotToolbarPresenter& toolbarPresenter;
    const ScreenshotGeometryMapper& geometry;
    ScreenshotDisplaySession& displaySession;
    ScreenshotInteractionState& interaction;
    ScreenshotSelectionModel& selection;
};

class ScreenshotPresentationServices final {
  public:
    explicit ScreenshotPresentationServices(ScreenshotPresentationServicesContext context);

    void hideToolbar();
    void hideMainToolbar();
    void showToolbar();
    void showSelectionToolbar();
    void moveToolbar();
    void repositionToolbarForContentChange();
    void raiseToolbarForCanvasInteraction();
    void setSelectionToolbarHovered(bool hovered);
    void setUiPreferences(const ScreenshotUiPreferences& preferences);

    void updateOverlayState();
    void updateOverlayCursors() const;

    [[nodiscard]] bool hasActiveDisplays() const;
    [[nodiscard]] QPoint physicalPositionForLogicalPoint(const QPointF& logicalPoint) const;
    [[nodiscard]] ScreenshotColorPickerContext colorPickerContext() const;

  private:
    void presentSelectionFrame(const QRectF& selection);
    void presentOverlayState(const QRectF& selection) const;
    [[nodiscard]] ScreenshotToolbarPresentationState toolbarPresentationState() const;

    ScreenshotPresentationServicesContext m_context;
    ScreenshotSmartSelectionTransition m_smartSelectionTransition;
    ScreenshotUiPreferences m_uiPreferences;
    bool m_selectionToolbarHovered = false;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTPRESENTATIONSERVICES_H
