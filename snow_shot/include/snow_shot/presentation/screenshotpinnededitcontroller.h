#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDEDITCONTROLLER_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDEDITCONTROLLER_H

#include <QObject>
#include <QPoint>
#include <QMap>
#include <QRect>

#include "snow_draw_engine_qt/snow_canvas_types.h"

class QScreen;
class QEvent;
class ScreenshotFloatingToolPaletteWindow;
class ScreenshotPinnedWindow;
class ScreenshotToolPaletteHost;
class SnowCanvasWidget;
namespace snow_shot::presentation {
class WindowShortcutManager;
}

class ScreenshotPinnedEditController final : public QObject {
    Q_OBJECT

  public:
    ScreenshotPinnedEditController(ScreenshotPinnedWindow& pinnedWindow, SnowCanvasWidget& canvas,
                                   snow_shot::presentation::WindowShortcutManager& shortcutManager,
                                   QObject* parent = nullptr);
    ~ScreenshotPinnedEditController() override;

    bool editMode() const;
    ScreenshotFloatingToolPaletteWindow* toolbarWindow() const;
    ScreenshotToolPaletteHost* toolbarHost() const;
    void setEditMode(bool enabled);
    void restoreDrawingToolState();
    void updatePlacement();
    void updateAfterPinnedWindowMove(const QPoint& logicalDelta);
    void raiseToolbar();
    void hideToolbar();
    void destroyToolbar();

  signals:
    void editModeChanged(bool enabled);

  private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void registerDrawingShortcuts();
    void reloadDrawingShortcuts();
    QScreen* placementScreen() const;
    QRect placementLogicalBounds() const;
    QRect placementPhysicalBounds() const;
    void syncPaletteFromCanvasTool();
    void syncPaletteFromCanvasStyle();
    void applyShapeStyleFromPalette(const SnowCanvasShapeStyle& style, quint32 properties,
                                    SnowCanvasShapeKind kind);
    void applyTextStyleFromPalette(const SnowCanvasTextStyle& style);
    void applySerialNumberStyleFromPalette(const SnowCanvasSerialNumberStyle& style);
    void markToolbarManuallyPlaced();

    ScreenshotPinnedWindow& m_pinnedWindow;
    SnowCanvasWidget& m_canvas;
    snow_shot::presentation::WindowShortcutManager& m_shortcutManager;
    QMap<QString, quint64> m_drawingShortcutBindings;
    ScreenshotFloatingToolPaletteWindow* m_toolbarWindow = nullptr;
    QPoint m_globalContentPosition;
    bool m_editMode = false;
    bool m_manuallyPlaced = false;
    bool m_updatingPlacement = false;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTPINNEDEDITCONTROLLER_H
