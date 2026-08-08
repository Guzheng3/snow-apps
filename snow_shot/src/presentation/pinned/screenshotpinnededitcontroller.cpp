#include "snow_shot/presentation/screenshotpinnededitcontroller.h"

#include "snow_shot/presentation/screenshotfloatingtoolpalettewindow.h"
#include "snow_shot/presentation/screenshotdefaultstyles.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotpinnedwindow.h"
#include "snow_shot/presentation/screenshottoolpalette.h"
#include "snow_shot/presentation/screenshottoolpalettehost.h"

#include "snow_draw_engine_qt/snow_canvas_widget.h"

#include <QEvent>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>

namespace {
constexpr int kToolbarGap = 4;
ScreenshotToolPalette::Options pinnedEditToolbarOptions() {
    ScreenshotToolPalette::Options options;
    options.showDragHandle = true;
    options.showHistoryActions = true;
    options.showSelectTool = true;
    options.showShapeTool = true;
    options.showArrowTool = true;
    options.showLineTool = true;
    options.showFreeDrawTool = true;
    options.showHighlightTool = true;
    options.showSpotlightTool = true;
    options.showEraserTool = true;
    options.showFilterTool = true;
    options.showWatermarkTool = true;
    options.showTextTool = true;
    options.showSerialNumberTool = true;
    options.showOcrTool = true;
    options.showTableTool = true;
    options.showQrTool = true;
    options.separatorAfterSelect = true;
    options.separatorBeforeConfirm = true;
    options.actions = ScreenshotToolPalette::ConfirmAction;
    options.styleDefaults = snow_shot::presentation::screenshotCanvasStyleDefaults();
    return options;
}
} // namespace

ScreenshotPinnedEditController::ScreenshotPinnedEditController(ScreenshotPinnedWindow& pinnedWindow,
                                                               SnowCanvasWidget& canvas,
                                                               QObject* parent)
    : QObject(parent), m_pinnedWindow(pinnedWindow), m_canvas(canvas) {
    m_canvas.installEventFilter(this);
    m_toolbarWindow = new ScreenshotFloatingToolPaletteWindow(pinnedEditToolbarOptions());
    m_toolbarWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    m_toolbarWindow->setTransientOwnerWindow(&m_pinnedWindow);
    m_toolbarWindow->setStyleToolbarAboveMain(false);

    if (ScreenshotToolPalette* toolbar = m_toolbarWindow->palette()) {
        toolbar->setHistoryState(m_canvas.canvasHistoryState());
        connect(toolbar, &ScreenshotToolPalette::undoRequested, this,
                [this]() { static_cast<void>(m_canvas.undo()); });
        connect(toolbar, &ScreenshotToolPalette::redoRequested, this,
                [this]() { static_cast<void>(m_canvas.redo()); });
        connect(toolbar, &ScreenshotToolPalette::selectRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Select); });
        connect(toolbar, &ScreenshotToolPalette::shapeRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Shape); });
        connect(toolbar, &ScreenshotToolPalette::arrowRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Arrow); });
        connect(toolbar, &ScreenshotToolPalette::lineRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Line); });
        connect(toolbar, &ScreenshotToolPalette::freeDrawRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::FreeDraw); });
        connect(toolbar, &ScreenshotToolPalette::highlightRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::RectangleHighlight); });
        connect(toolbar, &ScreenshotToolPalette::penHighlightRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::PenHighlight); });
        connect(toolbar, &ScreenshotToolPalette::spotlightRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Spotlight); });
        connect(toolbar, &ScreenshotToolPalette::eraserRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Eraser); });
        connect(toolbar, &ScreenshotToolPalette::filterRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Filter); });
        connect(toolbar, &ScreenshotToolPalette::rectangleFilterRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::RectangleFilter); });
        connect(toolbar, &ScreenshotToolPalette::penFilterRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::PenFilter); });
        connect(toolbar, &ScreenshotToolPalette::filterStyleChanged, this,
                [this](const SnowCanvasFilterStyle& style, quint32 properties) {
                    m_canvas.setCanvasFilterStyle(style, properties);
                });
        toolbar->setWatermarkConfig(m_canvas.canvasWatermarkConfig());
        toolbar->setSpotlightConfig(m_canvas.canvasSpotlightConfig());
        connect(toolbar, &ScreenshotToolPalette::watermarkRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Watermark); });
        connect(toolbar, &ScreenshotToolPalette::watermarkConfigChanged, this,
                [this](const SnowCanvasWatermarkConfig& config) {
                    m_canvas.setCanvasWatermarkConfig(config);
                });
        connect(toolbar, &ScreenshotToolPalette::watermarkPreviewChanged, this,
                [this](const SnowCanvasWatermarkConfig& config) {
                    m_canvas.previewCanvasWatermarkConfig(config);
                });
        connect(toolbar, &ScreenshotToolPalette::spotlightConfigChanged, this,
                [this](const SnowCanvasSpotlightConfig& config) {
                    m_canvas.setCanvasSpotlightConfig(config);
                });
        connect(toolbar, &ScreenshotToolPalette::spotlightPreviewChanged, this,
                [this](const SnowCanvasSpotlightConfig& config) {
                    m_canvas.previewCanvasSpotlightConfig(config);
                });
        connect(toolbar, &ScreenshotToolPalette::textRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::Text); });
        connect(toolbar, &ScreenshotToolPalette::serialNumberRequested, this,
                [this]() { m_canvas.setCanvasTool(SnowCanvasTool::SerialNumber); });
        connect(toolbar, &ScreenshotToolPalette::serialNumberDecrementRequested, this,
                [this]() { m_canvas.adjustSelectedSerialNumbers(-1); });
        connect(toolbar, &ScreenshotToolPalette::serialNumberIncrementRequested, this,
                [this]() { m_canvas.adjustSelectedSerialNumbers(1); });
        connect(toolbar, &ScreenshotToolPalette::serialNumberCreateTextRequested, this,
                [this]() { m_canvas.createSerialNumberText(); });
        connect(toolbar, &ScreenshotToolPalette::sendSelectionToBackRequested, this,
                [this]() { m_canvas.reorderSelected(SnowCanvasSelectionOrder::SendToBack); });
        connect(toolbar, &ScreenshotToolPalette::sendSelectionBackwardRequested, this,
                [this]() { m_canvas.reorderSelected(SnowCanvasSelectionOrder::SendBackward); });
        connect(toolbar, &ScreenshotToolPalette::bringSelectionForwardRequested, this,
                [this]() { m_canvas.reorderSelected(SnowCanvasSelectionOrder::BringForward); });
        connect(toolbar, &ScreenshotToolPalette::bringSelectionToFrontRequested, this,
                [this]() { m_canvas.reorderSelected(SnowCanvasSelectionOrder::BringToFront); });
        connect(toolbar, &ScreenshotToolPalette::selectionOpacityChanged, this,
                [this](qreal opacity) { m_canvas.setSelectedOpacity(opacity); });
        connect(toolbar, &ScreenshotToolPalette::duplicateSelectionRequested, this,
                [this]() { m_canvas.duplicateSelected(); });
        connect(toolbar, &ScreenshotToolPalette::deleteSelectionRequested, this,
                [this]() { m_canvas.deleteSelected(); });
        connect(toolbar, &ScreenshotToolPalette::shapeStyleChanged, this,
                &ScreenshotPinnedEditController::applyShapeStyleFromPalette);
        connect(toolbar, &ScreenshotToolPalette::textStyleChanged, this,
                &ScreenshotPinnedEditController::applyTextStyleFromPalette);
        connect(toolbar, &ScreenshotToolPalette::textStylePopupInteractionBegan, this,
                [this]() { m_canvas.beginTextStylePopupInteraction(); });
        connect(toolbar, &ScreenshotToolPalette::textStylePopupInteractionEnded, this,
                [this]() { m_canvas.endTextStylePopupInteraction(m_toolbarWindow); });
        connect(toolbar, &ScreenshotToolPalette::serialNumberStyleChanged, this,
                &ScreenshotPinnedEditController::applySerialNumberStyleFromPalette);
        connect(toolbar, &ScreenshotToolPalette::confirmRequested, this,
                [this]() { setEditMode(false); });
    }

    connect(m_toolbarWindow, &ScreenshotFloatingToolPaletteWindow::dragFinished, this,
            &ScreenshotPinnedEditController::markToolbarManuallyPlaced);
    if (ScreenshotToolPaletteHost* host = m_toolbarWindow->paletteHost()) {
        connect(host, &ScreenshotToolPaletteHost::dragStarted, this,
                [this](const QPoint&) { markToolbarManuallyPlaced(); });
    }
    connect(&m_canvas, &SnowCanvasWidget::activeToolChanged, this,
            &ScreenshotPinnedEditController::syncPaletteFromCanvasTool);
    connect(&m_canvas, &SnowCanvasWidget::styleToolbarStateChanged, this,
            &ScreenshotPinnedEditController::syncPaletteFromCanvasStyle);
    connect(&m_canvas, &SnowCanvasWidget::historyStateChanged, this, [this]() {
        if (m_toolbarWindow != nullptr) {
            if (ScreenshotToolPalette* toolbar = m_toolbarWindow->palette()) {
                toolbar->setHistoryState(m_canvas.canvasHistoryState());
            }
        }
    });

    updatePlacement();
}

ScreenshotPinnedEditController::~ScreenshotPinnedEditController() {
    destroyToolbar();
}

bool ScreenshotPinnedEditController::editMode() const {
    return m_editMode;
}

bool ScreenshotPinnedEditController::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &m_canvas && event != nullptr && event->type() == QEvent::Wheel && m_editMode) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        const int deltaY = !wheelEvent->pixelDelta().isNull() ? wheelEvent->pixelDelta().y()
                                                              : wheelEvent->angleDelta().y();
        ScreenshotToolPaletteHost* host = toolbarHost();
        const int direction = deltaY > 0 ? 1 : -1;
        bool handled = false;
        if (deltaY != 0 && host != nullptr) {
            switch (m_canvas.canvasTool()) {
            case SnowCanvasTool::Spotlight:
                handled = host->stepSpotlightOpacity(direction);
                break;
            case SnowCanvasTool::RectangleFilter:
                handled = host->stepFilterIntensity(direction);
                break;
            case SnowCanvasTool::PenFilter:
                handled = host->stepPenFilterStrokeWidth(direction);
                break;
            case SnowCanvasTool::Watermark:
                handled = host->stepWatermarkFontSize(direction);
                break;
            default:
                break;
            }
        }
        if (handled) {
            wheelEvent->accept();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

ScreenshotFloatingToolPaletteWindow* ScreenshotPinnedEditController::toolbarWindow() const {
    return m_toolbarWindow;
}

ScreenshotToolPaletteHost* ScreenshotPinnedEditController::toolbarHost() const {
    return m_toolbarWindow != nullptr ? m_toolbarWindow->paletteHost() : nullptr;
}

void ScreenshotPinnedEditController::setEditMode(bool enabled) {
    if (m_editMode == enabled && m_canvas.interactionEnabled() == enabled) {
        return;
    }

    m_editMode = enabled;
    if (enabled) {
        m_canvas.setInteractionEnabled(true);
        m_canvas.setFocus(Qt::OtherFocusReason);
        m_canvas.setCanvasTool(SnowCanvasTool::Select);
        syncPaletteFromCanvasStyle();
        m_manuallyPlaced = false;
        if (m_toolbarWindow != nullptr) {
            m_toolbarWindow->cancelDrag();
            if (ScreenshotToolPaletteHost* host = m_toolbarWindow->paletteHost()) {
                host->setActiveTool(ScreenshotToolPalette::Tool::Select);
            }
            updatePlacement();
            m_toolbarWindow->prepareForDisplay();
            m_toolbarWindow->show();
            raiseToolbar();
        }
        emit editModeChanged(true);
        return;
    }

    static_cast<void>(m_canvas.resetEditingState());
    m_canvas.setInteractionEnabled(false);
    m_canvas.clearFocus();
    if (m_toolbarWindow != nullptr) {
        m_toolbarWindow->cancelDrag();
        if (ScreenshotToolPaletteHost* host = m_toolbarWindow->paletteHost()) {
            host->clearActiveTool();
        }
        hideToolbar();
    }
    updatePlacement();
    emit editModeChanged(false);
}

void ScreenshotPinnedEditController::updatePlacement() {
    if (m_toolbarWindow == nullptr || m_updatingPlacement) {
        return;
    }

    const QRect logicalBounds = placementLogicalBounds();
    const QRect physicalBounds = placementPhysicalBounds();
    m_toolbarWindow->setPlacementContext(placementScreen(), logicalBounds, physicalBounds);
    m_toolbarWindow->prepareForDisplay();

    const QRect bottomToolbarRect = m_toolbarWindow->bottomPlacementContentRect();
    if (bottomToolbarRect.isEmpty()) {
        return;
    }

    if (!m_manuallyPlaced) {
        const QRect pinnedGeometry =
            m_pinnedWindow.frameGeometry().isValid() && !m_pinnedWindow.frameGeometry().isEmpty()
                ? m_pinnedWindow.frameGeometry()
                : m_pinnedWindow.geometry();
        QRect placementBounds;
        if (const QScreen* screen = placementScreen()) {
            placementBounds = screen->geometry();
        }
        if (!placementBounds.isValid() || placementBounds.isEmpty()) {
            placementBounds = pinnedGeometry;
        }
        const ScreenshotAnchoredToolbarPlacement placement =
            ScreenshotGeometryMapper::anchoredToolbarPlacement(
                QPoint(pinnedGeometry.left() + pinnedGeometry.width(),
                       pinnedGeometry.top() + pinnedGeometry.height()),
                QPoint(pinnedGeometry.left() + pinnedGeometry.width(), pinnedGeometry.top()),
                bottomToolbarRect, placementBounds, kToolbarGap,
                m_toolbarWindow->topRightMainToolbarContentRect(),
                m_toolbarWindow->topPlacementContentRect());
        m_toolbarWindow->setStyleToolbarAboveMain(placement.usesTopRightPlacement);
        m_globalContentPosition = placement.contentPosition;
        m_toolbarWindow->resetPhysicalSizeInvariant();
    }

    m_updatingPlacement = true;
    m_toolbarWindow->moveContentTo(m_globalContentPosition);
    m_updatingPlacement = false;
    if (m_toolbarWindow->isVisible()) {
        raiseToolbar();
    }
}

void ScreenshotPinnedEditController::updateAfterPinnedWindowMove(const QPoint& logicalDelta) {
    if (m_manuallyPlaced) {
        m_globalContentPosition += logicalDelta;
    }
    updatePlacement();
}

void ScreenshotPinnedEditController::raiseToolbar() {
    if (m_toolbarWindow != nullptr && m_toolbarWindow->isVisible()) {
        m_toolbarWindow->raise();
    }
}

void ScreenshotPinnedEditController::hideToolbar() {
    if (m_toolbarWindow == nullptr) {
        return;
    }

    m_toolbarWindow->cancelDrag();
    m_toolbarWindow->hide();
}

void ScreenshotPinnedEditController::destroyToolbar() {
    if (m_toolbarWindow == nullptr) {
        return;
    }

    ScreenshotFloatingToolPaletteWindow* toolbarWindow = m_toolbarWindow;
    m_toolbarWindow = nullptr;
    toolbarWindow->cancelDrag();
    toolbarWindow->setTransientOwnerWindow(nullptr);
    toolbarWindow->hide();
    delete toolbarWindow;
}

QScreen* ScreenshotPinnedEditController::placementScreen() const {
    if (QWindow* pinnedHandle = m_pinnedWindow.windowHandle()) {
        if (pinnedHandle->screen() != nullptr) {
            return pinnedHandle->screen();
        }
    }
    return m_pinnedWindow.screen();
}

QRect ScreenshotPinnedEditController::placementLogicalBounds() const {
    if (QScreen* screen = placementScreen()) {
        const QRect screenGeometry = screen->geometry();
        if (screenGeometry.isValid() && !screenGeometry.isEmpty()) {
            return screenGeometry;
        }
    }

    QRect logicalBounds = m_pinnedWindow.frameGeometry();
    if (!logicalBounds.isValid() || logicalBounds.isEmpty()) {
        logicalBounds = m_pinnedWindow.geometry();
    }
    return logicalBounds;
}

QRect ScreenshotPinnedEditController::placementPhysicalBounds() const {
    if (QScreen* screen = placementScreen()) {
        const QRect screenPhysicalBounds = ScreenshotGeometryMapper::physicalRectForScreen(*screen);
        if (screenPhysicalBounds.isValid() && !screenPhysicalBounds.isEmpty()) {
            return screenPhysicalBounds;
        }
    }

    QRect physicalBounds = m_pinnedWindow.currentNativeGeometry();
    if (physicalBounds.isValid() && !physicalBounds.isEmpty()) {
        return physicalBounds;
    }
    return placementLogicalBounds();
}

void ScreenshotPinnedEditController::syncPaletteFromCanvasTool() {
    ScreenshotToolPaletteHost* host = toolbarHost();
    if (host == nullptr) {
        return;
    }

    switch (m_canvas.canvasTool()) {
    case SnowCanvasTool::Select:
        host->setActiveTool(ScreenshotToolPalette::Tool::Select);
        break;
    case SnowCanvasTool::Shape:
        host->setActiveTool(ScreenshotToolPalette::Tool::Shape);
        break;
    case SnowCanvasTool::Arrow:
        host->setActiveTool(ScreenshotToolPalette::Tool::Arrow);
        break;
    case SnowCanvasTool::Line:
        host->setActiveTool(ScreenshotToolPalette::Tool::Line);
        break;
    case SnowCanvasTool::FreeDraw:
        host->setActiveTool(ScreenshotToolPalette::Tool::FreeDraw);
        break;
    case SnowCanvasTool::RectangleHighlight:
        host->setActiveTool(ScreenshotToolPalette::Tool::RectangleHighlight);
        break;
    case SnowCanvasTool::PenHighlight:
        host->setActiveTool(ScreenshotToolPalette::Tool::PenHighlight);
        break;
    case SnowCanvasTool::Spotlight:
        host->setActiveTool(ScreenshotToolPalette::Tool::Spotlight);
        break;
    case SnowCanvasTool::Eraser:
        host->setActiveTool(ScreenshotToolPalette::Tool::Eraser);
        break;
    case SnowCanvasTool::RectangleFilter:
        host->setActiveTool(ScreenshotToolPalette::Tool::RectangleFilter);
        break;
    case SnowCanvasTool::PenFilter:
        host->setActiveTool(ScreenshotToolPalette::Tool::PenFilter);
        break;
    case SnowCanvasTool::Watermark:
        host->setActiveTool(ScreenshotToolPalette::Tool::Watermark);
        break;
    case SnowCanvasTool::Text:
        host->setActiveTool(ScreenshotToolPalette::Tool::Text);
        break;
    case SnowCanvasTool::SerialNumber:
        host->setActiveTool(ScreenshotToolPalette::Tool::SerialNumber);
        break;
    default:
        host->clearActiveTool();
        break;
    }
}

void ScreenshotPinnedEditController::syncPaletteFromCanvasStyle() {
    ScreenshotToolPalette* toolbar =
        m_toolbarWindow != nullptr ? m_toolbarWindow->palette() : nullptr;
    if (toolbar == nullptr) {
        return;
    }

    toolbar->setStyleToolbarState(m_canvas.canvasStyleToolbarState());
    toolbar->setWatermarkConfig(m_canvas.canvasWatermarkConfig());
    toolbar->setSpotlightConfig(m_canvas.canvasSpotlightConfig());
}

void ScreenshotPinnedEditController::applyShapeStyleFromPalette(const SnowCanvasShapeStyle& style,
                                                                quint32 properties,
                                                                SnowCanvasShapeKind kind) {
    m_canvas.setCanvasShapeStylePatch(style, properties, kind);
}

void ScreenshotPinnedEditController::applyTextStyleFromPalette(const SnowCanvasTextStyle& style) {
    static_cast<void>(m_canvas.setCanvasTextStyle(style));
}

void ScreenshotPinnedEditController::applySerialNumberStyleFromPalette(
    const SnowCanvasSerialNumberStyle& style) {
    static_cast<void>(m_canvas.setCanvasSerialNumberStyle(style));
}

void ScreenshotPinnedEditController::markToolbarManuallyPlaced() {
    if (m_toolbarWindow == nullptr) {
        return;
    }

    m_manuallyPlaced = true;
    m_globalContentPosition = m_toolbarWindow->contentPosition();
}
