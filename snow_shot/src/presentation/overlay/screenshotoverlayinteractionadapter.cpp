#include "snow_shot/presentation/screenshotoverlayinteractionadapter.h"

#include "snow_shot/presentation/screenshotoverlayinputhandler.h"

#include <utility>

void ScreenshotOverlayEventAdapter::setEventTargets(
    ScreenshotOverlayInputHandler& inputHandler,
    std::function<void()> raiseToolbarForCanvasInteraction) {
    m_inputHandler = &inputHandler;
    m_raiseToolbarForCanvasInteraction = std::move(raiseToolbarForCanvasInteraction);
}

void ScreenshotOverlayEventAdapter::clearEventTargets() {
    m_inputHandler = nullptr;
    m_raiseToolbarForCanvasInteraction = nullptr;
}

bool ScreenshotOverlayEventAdapter::shouldHandleOverlayMouseEvent(
    const ScreenshotOverlayWindow* overlay, const QPointF& localPosition,
    bool leftButtonActive) const {
    if (m_inputHandler == nullptr) {
        return false;
    }
    return m_inputHandler->shouldHandleMouseEvent(overlay, localPosition, leftButtonActive);
}

void ScreenshotOverlayEventAdapter::handleOverlayMousePress(ScreenshotOverlayWindow* overlay,
                                                            const QPointF& localPosition) {
    if (m_inputHandler != nullptr) {
        m_inputHandler->handleMousePress(overlay, localPosition);
    }
}

void ScreenshotOverlayEventAdapter::handleOverlayMouseMove(ScreenshotOverlayWindow* overlay,
                                                           const QPointF& localPosition) {
    if (m_inputHandler != nullptr) {
        m_inputHandler->handleMouseMove(overlay, localPosition);
    }
}

void ScreenshotOverlayEventAdapter::handleOverlayMouseRelease(ScreenshotOverlayWindow* overlay,
                                                              const QPointF& localPosition) {
    if (m_inputHandler != nullptr) {
        m_inputHandler->handleMouseRelease(overlay, localPosition);
    }
}

bool ScreenshotOverlayEventAdapter::handleOverlayRightClick(ScreenshotOverlayWindow* overlay,
                                                            const QPointF& localPosition) {
    if (m_inputHandler == nullptr) {
        return false;
    }
    return m_inputHandler->handleRightClick(overlay, localPosition);
}

bool ScreenshotOverlayEventAdapter::handleOverlayWheel(ScreenshotOverlayWindow* overlay,
                                                       const QPointF& localPosition,
                                                       const QPoint& angleDelta,
                                                       const QPoint& pixelDelta) {
    if (m_inputHandler == nullptr) {
        return false;
    }
    return m_inputHandler->handleWheel(overlay, localPosition, angleDelta, pixelDelta);
}

bool ScreenshotOverlayEventAdapter::handleOverlayKeyPress(int key,
                                                          Qt::KeyboardModifiers modifiers) {
    if (m_inputHandler == nullptr) {
        return false;
    }
    return m_inputHandler->handleKeyPress(key, modifiers);
}

void ScreenshotOverlayEventAdapter::raiseToolbarForCanvasInteraction() {
    if (m_raiseToolbarForCanvasInteraction) {
        m_raiseToolbarForCanvasInteraction();
    }
}
