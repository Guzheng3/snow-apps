#include "snow_shot/presentation/screenshotoverlayuihost.h"

#include "snow_draw_engine_qt/snow_canvas_widget.h"
#include "snow_shot/presentation/screenshotcolorpickerwidget.h"
#include "snow_shot/presentation/screenshotselectiontoolbarwindow.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"
#include "snow_shot/presentation/screenshottoolbarcommands.h"
#include "snow_shot/presentation/screenshottoolbarwindow.h"

#include <QCursor>
#include <QGuiApplication>
#include <QWidget>

namespace {
template <typename Widget> Widget* trackedWidget(QPointer<Widget>& pointer) {
    return pointer.data();
}
} // namespace

ScreenshotOverlayUiHost::ScreenshotOverlayUiHost() = default;

ScreenshotOverlayUiHost::~ScreenshotOverlayUiHost() {
    destroyUiResources();
}

void ScreenshotOverlayUiHost::setToolbarCommandSinks(
    ScreenshotToolbarCommandSink& toolbarCommands,
    ScreenshotSelectionToolbarCommandSink& selectionToolbarCommands) {
    m_toolbarCommands = &toolbarCommands;
    m_selectionToolbarCommands = &selectionToolbarCommands;
}

ScreenshotToolbarWindow* ScreenshotOverlayUiHost::ensureToolbar() {
    if (m_toolbar == nullptr) {
        if (m_toolbarCommands == nullptr) {
            return nullptr;
        }
        auto* toolbar = new ScreenshotToolbarWindow(*m_toolbarCommands);
        m_ownedWidgets.add(toolbar);
        m_toolbar = toolbar;
    }
    return trackedWidget(m_toolbar);
}

ScreenshotToolbarWindow* ScreenshotOverlayUiHost::toolbar() const {
    return m_toolbar.data();
}

void ScreenshotOverlayUiHost::prewarmToolbar() {
    if (ScreenshotToolbarWindow* toolbarWindow = ensureToolbar()) {
        toolbarWindow->prewarmForScreen(QGuiApplication::primaryScreen());
    }
}

void ScreenshotOverlayUiHost::attachToolbarToOverlay(ScreenshotOverlayWindow* overlay) {
    ScreenshotToolbarWindow* toolbarWindow = ensureToolbar();
    if (toolbarWindow == nullptr) {
        return;
    }

    if (m_toolbarStyleConnection) {
        if (m_toolbarStyleCanvas != nullptr) {
            m_toolbarStyleCanvas->endTextStylePopupInteraction(toolbarWindow);
        }
        QObject::disconnect(m_toolbarStyleConnection);
        m_toolbarStyleConnection = {};
    }
    if (m_toolbarHistoryConnection) {
        QObject::disconnect(m_toolbarHistoryConnection);
        m_toolbarHistoryConnection = {};
    }
    if (m_toolbarStylePopupBeginConnection) {
        QObject::disconnect(m_toolbarStylePopupBeginConnection);
        m_toolbarStylePopupBeginConnection = {};
    }
    if (m_toolbarStylePopupEndConnection) {
        QObject::disconnect(m_toolbarStylePopupEndConnection);
        m_toolbarStylePopupEndConnection = {};
    }
    m_toolbarStyleCanvas = nullptr;
    toolbarWindow->setOwnerWindow(overlay);

    if (overlay == nullptr || overlay->canvas() == nullptr) {
        return;
    }

    SnowCanvasWidget* canvas = overlay->canvas();
    m_toolbarStyleCanvas = canvas;
    toolbarWindow->setStyleToolbarState(canvas->canvasStyleToolbarState());
    toolbarWindow->setHistoryState(canvas->canvasHistoryState());
    toolbarWindow->setWatermarkConfig(canvas->canvasWatermarkConfig());
    toolbarWindow->setSpotlightConfig(canvas->canvasSpotlightConfig());
    m_toolbarStyleConnection =
        QObject::connect(canvas, &SnowCanvasWidget::styleToolbarStateChanged, toolbarWindow,
                         [toolbarWindow, canvas]() {
                             toolbarWindow->setStyleToolbarState(canvas->canvasStyleToolbarState());
                             toolbarWindow->setWatermarkConfig(canvas->canvasWatermarkConfig());
                             toolbarWindow->setSpotlightConfig(canvas->canvasSpotlightConfig());
                         });
    m_toolbarHistoryConnection = QObject::connect(
        canvas, &SnowCanvasWidget::historyStateChanged, toolbarWindow, [toolbarWindow, canvas]() {
            toolbarWindow->setHistoryState(canvas->canvasHistoryState());
        });
    if (ScreenshotToolPalette* palette = toolbarWindow->palette()) {
        m_toolbarStylePopupBeginConnection = QObject::connect(
            palette, &ScreenshotToolPalette::textStylePopupInteractionBegan, toolbarWindow,
            [canvas]() { canvas->beginTextStylePopupInteraction(); });
        m_toolbarStylePopupEndConnection = QObject::connect(
            palette, &ScreenshotToolPalette::textStylePopupInteractionEnded, toolbarWindow,
            [canvas, toolbarWindow]() { canvas->endTextStylePopupInteraction(toolbarWindow); });
    }
}

void ScreenshotOverlayUiHost::undoCanvasEdit() {
    if (m_toolbarStyleCanvas != nullptr) {
        static_cast<void>(m_toolbarStyleCanvas->undo());
    }
}

void ScreenshotOverlayUiHost::redoCanvasEdit() {
    if (m_toolbarStyleCanvas != nullptr) {
        static_cast<void>(m_toolbarStyleCanvas->redo());
    }
}

ScreenshotSelectionToolbarWindow* ScreenshotOverlayUiHost::ensureSelectionToolbar() {
    if (m_selectionToolbar == nullptr) {
        if (m_selectionToolbarCommands == nullptr) {
            return nullptr;
        }
        auto* selectionToolbar = new ScreenshotSelectionToolbarWindow(*m_selectionToolbarCommands);
        m_ownedWidgets.add(selectionToolbar);
        m_selectionToolbar = selectionToolbar;
        selectionToolbar->hide();
    }
    return trackedWidget(m_selectionToolbar);
}

ScreenshotSelectionToolbarWindow* ScreenshotOverlayUiHost::selectionToolbar() const {
    return m_selectionToolbar.data();
}

void ScreenshotOverlayUiHost::attachSelectionToolbarToOverlay(ScreenshotOverlayWindow* overlay) {
    ScreenshotSelectionToolbarWindow* toolbarWindow = ensureSelectionToolbar();
    if (toolbarWindow == nullptr) {
        return;
    }
    if (toolbarWindow->parentWidget() == overlay) {
        return;
    }

    const bool wasVisible = toolbarWindow->isVisible();
    toolbarWindow->hide();
    toolbarWindow->setParent(overlay);
    toolbarWindow->setWindowFlags(Qt::FramelessWindowHint);
    toolbarWindow->setAttribute(Qt::WA_TranslucentBackground, true);
    toolbarWindow->setFocusPolicy(Qt::NoFocus);
    if (wasVisible && overlay != nullptr && overlay->isVisible()) {
        toolbarWindow->show();
        toolbarWindow->raise();
    }
}

ScreenshotColorPickerWidget* ScreenshotOverlayUiHost::ensureColorPicker() {
    if (m_colorPicker == nullptr) {
        auto* colorPicker = new ScreenshotColorPickerWidget();
        m_ownedWidgets.add(colorPicker);
        m_colorPicker = colorPicker;
        colorPicker->hide();
    }
    return trackedWidget(m_colorPicker);
}

ScreenshotColorPickerWidget* ScreenshotOverlayUiHost::colorPicker() const {
    return m_colorPicker.data();
}

void ScreenshotOverlayUiHost::updateColorPicker(ScreenshotOverlayWindow* overlay,
                                                const QImage& image, const QRect& physicalRect,
                                                const QPoint& physicalPoint,
                                                const QPointF& localPosition, qreal opacity) {
    if (overlay == nullptr) {
        hideColorPicker();
        return;
    }

    ScreenshotColorPickerWidget* picker = ensureColorPicker();
    SnowCanvasWidget* canvas = overlay->canvas();
    QWidget* pickerParent =
        canvas != nullptr ? static_cast<QWidget*>(canvas) : static_cast<QWidget*>(overlay);
    if (picker->parentWidget() != pickerParent) {
        picker->hidePicker();
        picker->setParent(pickerParent);
        picker->setAttribute(Qt::WA_TranslucentBackground, true);
        picker->setAttribute(Qt::WA_NoSystemBackground, true);
        picker->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        picker->setFocusPolicy(Qt::NoFocus);
    }

    picker->setCaptureImage(image, physicalRect);
    picker->updatePicker(physicalPoint, localPosition, opacity);
}

void ScreenshotOverlayUiHost::hideColorPicker() {
    if (m_colorPicker != nullptr) {
        m_colorPicker->hidePicker();
    }
}

void ScreenshotOverlayUiHost::resetColorPickerForNewCapture() {
    if (m_colorPicker != nullptr) {
        m_colorPicker->resetForNewCapture();
    }
}

void ScreenshotOverlayUiHost::hideColorPickerForOverlay(ScreenshotOverlayWindow* overlay) const {
    if (colorPickerBelongsToOverlay(overlay)) {
        m_colorPicker->hidePicker();
    }
}

bool ScreenshotOverlayUiHost::colorPickerBelongsToOverlay(
    const ScreenshotOverlayWindow* overlay) const {
    if (m_colorPicker == nullptr || overlay == nullptr) {
        return false;
    }

    const QWidget* parent = m_colorPicker->parentWidget();
    return parent == overlay || parent == overlay->canvas();
}

bool ScreenshotOverlayUiHost::screenshotUiContainsGlobalCursor() const {
    const QPoint globalPosition = QCursor::pos();
    if (m_toolbar != nullptr && m_toolbar->isVisible() &&
        m_toolbar->containsInteractiveGlobalPoint(globalPosition)) {
        return true;
    }

    if (m_selectionToolbar != nullptr && m_selectionToolbar->isVisible() &&
        !m_selectionToolbar->testAttribute(Qt::WA_TransparentForMouseEvents) &&
        m_selectionToolbar->containsInteractiveGlobalPoint(globalPosition)) {
        return true;
    }

    return false;
}

bool ScreenshotOverlayUiHost::stepToolbarStrokeWidth(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepStrokeWidth(direction);
}

bool ScreenshotOverlayUiHost::stepToolbarSelectionOpacity(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepSelectionOpacity(direction);
}

bool ScreenshotOverlayUiHost::stepToolbarSpotlightOpacity(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepSpotlightOpacity(direction);
}

bool ScreenshotOverlayUiHost::stepToolbarFilterIntensity(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepFilterIntensity(direction);
}

bool ScreenshotOverlayUiHost::stepToolbarPenFilterStrokeWidth(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepPenFilterStrokeWidth(direction);
}

bool ScreenshotOverlayUiHost::stepToolbarWatermarkFontSize(int direction) {
    return m_toolbar != nullptr && m_toolbar->stepWatermarkFontSize(direction);
}

void ScreenshotOverlayUiHost::resetToolbarForNewCapture() {
    if (m_toolbar != nullptr) {
        const bool wasVisible = m_toolbar->isVisible();
        m_toolbar->resetForNewCapture();
        if (wasVisible) {
            // Refresh the translucent native surface before it is hidden. Otherwise
            // Windows can briefly reuse the previous frame when the toolbar is shown
            // for the next capture.
            m_toolbar->repaint();
        }
    }
    if (m_selectionToolbar != nullptr) {
        m_selectionToolbar->resetForNewCapture();
    }
}

void ScreenshotOverlayUiHost::hideToolbar() {
    if (m_toolbar != nullptr) {
        m_toolbar->hide();
    }
    hideSelectionToolbar();
}

void ScreenshotOverlayUiHost::showToolbar() {
    ScreenshotToolbarWindow* toolbarWindow = ensureToolbar();
    if (toolbarWindow == nullptr) {
        return;
    }
    toolbarWindow->prepareForDisplay();
    toolbarWindow->show();
    toolbarWindow->raise();
}

void ScreenshotOverlayUiHost::raiseToolbar() {
    if (m_toolbar != nullptr) {
        m_toolbar->raise();
    }
    raiseSelectionToolbar();
}

void ScreenshotOverlayUiHost::hideSelectionToolbar() {
    if (m_selectionToolbar != nullptr) {
        m_selectionToolbar->hide();
    }
}

void ScreenshotOverlayUiHost::showSelectionToolbar() {
    ScreenshotSelectionToolbarWindow* toolbarWindow = ensureSelectionToolbar();
    if (toolbarWindow == nullptr) {
        return;
    }
    toolbarWindow->prepareForDisplay();
    toolbarWindow->show();
    toolbarWindow->raise();
}

void ScreenshotOverlayUiHost::raiseSelectionToolbar() {
    if (m_selectionToolbar != nullptr) {
        m_selectionToolbar->raise();
    }
}

void ScreenshotOverlayUiHost::detachOverlayTransientUi(ScreenshotOverlayWindow* overlay) {
    if (overlay == nullptr) {
        return;
    }

    if (m_selectionToolbar != nullptr && m_selectionToolbar->parentWidget() == overlay) {
        m_selectionToolbar->hide();
        m_selectionToolbar->setParent(nullptr);
        m_selectionToolbar->setWindowFlags(Qt::FramelessWindowHint);
    }

    if (m_toolbarStyleCanvas == overlay->canvas() && m_toolbarStyleConnection) {
        m_toolbarStyleCanvas->endTextStylePopupInteraction(m_toolbar.data());
        QObject::disconnect(m_toolbarStyleConnection);
        m_toolbarStyleConnection = {};
        if (m_toolbarHistoryConnection) {
            QObject::disconnect(m_toolbarHistoryConnection);
            m_toolbarHistoryConnection = {};
        }
        if (m_toolbarStylePopupBeginConnection) {
            QObject::disconnect(m_toolbarStylePopupBeginConnection);
            m_toolbarStylePopupBeginConnection = {};
        }
        if (m_toolbarStylePopupEndConnection) {
            QObject::disconnect(m_toolbarStylePopupEndConnection);
            m_toolbarStylePopupEndConnection = {};
        }
        m_toolbarStyleCanvas = nullptr;
    }
    if (m_toolbar != nullptr && m_toolbar->parentWidget() == overlay) {
        m_toolbar->hide();
        m_toolbar->setOwnerWindow(nullptr);
    }
    if (colorPickerBelongsToOverlay(overlay)) {
        m_colorPicker->hidePicker();
        m_colorPicker->setParent(nullptr);
    }
}

void ScreenshotOverlayUiHost::destroyUiResources() {
    if (m_toolbarStyleConnection) {
        if (m_toolbarStyleCanvas != nullptr) {
            m_toolbarStyleCanvas->endTextStylePopupInteraction(m_toolbar.data());
        }
        QObject::disconnect(m_toolbarStyleConnection);
        m_toolbarStyleConnection = {};
    }
    if (m_toolbarHistoryConnection) {
        QObject::disconnect(m_toolbarHistoryConnection);
        m_toolbarHistoryConnection = {};
    }
    if (m_toolbarStylePopupBeginConnection) {
        QObject::disconnect(m_toolbarStylePopupBeginConnection);
        m_toolbarStylePopupBeginConnection = {};
    }
    if (m_toolbarStylePopupEndConnection) {
        QObject::disconnect(m_toolbarStylePopupEndConnection);
        m_toolbarStylePopupEndConnection = {};
    }
    m_toolbarStyleCanvas = nullptr;

    if (m_toolbar != nullptr) {
        m_toolbar->hide();
    }

    if (m_selectionToolbar != nullptr) {
        m_selectionToolbar->hide();
    }

    if (m_colorPicker != nullptr) {
        m_colorPicker->hidePicker();
    }

    m_ownedWidgets.clear();
    m_toolbar = nullptr;
    m_selectionToolbar = nullptr;
    m_colorPicker = nullptr;
}
