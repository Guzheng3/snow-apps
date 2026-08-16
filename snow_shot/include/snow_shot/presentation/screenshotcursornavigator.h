#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTCURSORNAVIGATOR_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTCURSORNAVIGATOR_H

#include <QPoint>

#include <functional>
#include <optional>

class ScreenshotDisplaySession;
class ScreenshotGeometryMapper;

class ScreenshotCursorNavigator final {
  public:
    using PositionReader = std::function<std::optional<QPoint>()>;
    using PositionWriter = std::function<bool(const QPoint&)>;

    ScreenshotCursorNavigator(const ScreenshotGeometryMapper& geometry,
                              const ScreenshotDisplaySession& displaySession,
                              PositionReader positionReader = {},
                              PositionWriter positionWriter = {});

    [[nodiscard]] std::optional<QPoint> currentPhysicalPosition() const;
    [[nodiscard]] std::optional<QPoint> moveBy(const QPoint& delta) const;

  private:
    [[nodiscard]] bool setPhysicalPosition(const QPoint& position) const;

    const ScreenshotGeometryMapper& m_geometry;
    const ScreenshotDisplaySession& m_displaySession;
    PositionReader m_positionReader;
    PositionWriter m_positionWriter;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTCURSORNAVIGATOR_H
