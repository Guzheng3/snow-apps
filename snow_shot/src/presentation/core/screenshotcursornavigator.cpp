#include "snow_shot/presentation/screenshotcursornavigator.h"

#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"

#include <QCursor>

#include <utility>

#if defined(Q_OS_WIN) || defined(_WIN32)
#include <qt_windows.h>
#endif

ScreenshotCursorNavigator::ScreenshotCursorNavigator(const ScreenshotGeometryMapper& geometry,
                                                     const ScreenshotDisplaySession& displaySession,
                                                     PositionReader positionReader,
                                                     PositionWriter positionWriter)
    : m_geometry(geometry), m_displaySession(displaySession),
      m_positionReader(std::move(positionReader)), m_positionWriter(std::move(positionWriter)) {}

std::optional<QPoint> ScreenshotCursorNavigator::currentPhysicalPosition() const {
    if (m_positionReader) {
        return m_positionReader();
    }

#if defined(Q_OS_WIN) || defined(_WIN32)
    POINT position{};
    if (GetCursorPos(&position) == FALSE) {
        return std::nullopt;
    }
    return QPoint(position.x, position.y);
#else
    return m_geometry.physicalPositionForLogicalPoint(m_displaySession, QCursor::pos());
#endif
}

std::optional<QPoint> ScreenshotCursorNavigator::moveBy(const QPoint& delta) const {
    if (delta.isNull() || m_displaySession.isEmpty()) {
        return std::nullopt;
    }

    const std::optional<QPoint> currentPosition = currentPhysicalPosition();
    if (!currentPosition.has_value()) {
        return std::nullopt;
    }

    const CapturedDisplayModel* currentDisplay =
        m_geometry.displayForPhysicalPoint(m_displaySession, currentPosition.value());
    QPoint target = m_geometry.clampPhysicalPointToDesktop(currentPosition.value() + delta);

    // The desktop bounding rectangle can contain gaps between monitors. Keep a
    // nudge at an inside edge on its current display instead of entering a gap.
    if (m_geometry.displayForPhysicalPoint(m_displaySession, target) == nullptr &&
        currentDisplay != nullptr) {
        target = m_geometry.clampPhysicalPointToDisplay(*currentDisplay, target);
    }

    if (target == currentPosition.value()) {
        return target;
    }
    if (!setPhysicalPosition(target)) {
        return std::nullopt;
    }
    return target;
}

bool ScreenshotCursorNavigator::setPhysicalPosition(const QPoint& position) const {
    if (m_positionWriter) {
        return m_positionWriter(position);
    }

#if defined(Q_OS_WIN) || defined(_WIN32)
    return SetCursorPos(position.x(), position.y()) != FALSE;
#else
    const CapturedDisplayModel* display =
        m_geometry.displayForPhysicalPoint(m_displaySession, position);
    if (display == nullptr) {
        return false;
    }
    QCursor::setPos(m_geometry.logicalPositionForPhysicalPoint(*display, position).toPoint());
    return true;
#endif
}
