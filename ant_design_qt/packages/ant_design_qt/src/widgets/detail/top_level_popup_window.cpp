#include "top_level_popup_window.h"

#include <QWidget>
#include <QWindow>

namespace adqt::widgets::detail {

void syncTopLevelToolTransientParent(QWidget* toolWindow, QWidget* ownerWindow) {
  if (!toolWindow || !ownerWindow || !toolWindow->isWindow()) {
    return;
  }

  QWidget* ownerTopLevel = ownerWindow->window();
  if (!ownerTopLevel || ownerTopLevel == toolWindow) {
    return;
  }

  const bool ownerStaysOnTop = ownerTopLevel->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
  if (toolWindow->windowFlags().testFlag(Qt::WindowStaysOnTopHint) != ownerStaysOnTop) {
    toolWindow->setWindowFlag(Qt::WindowStaysOnTopHint, ownerStaysOnTop);
  }

  ownerTopLevel->winId();
  toolWindow->winId();
  QWindow* ownerHandle = ownerTopLevel->windowHandle();
  QWindow* toolHandle = toolWindow->windowHandle();
  if (ownerHandle && toolHandle && toolHandle->transientParent() != ownerHandle) {
    toolHandle->setTransientParent(ownerHandle);
  }
}

}  // namespace adqt::widgets::detail
