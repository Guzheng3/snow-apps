#pragma once

class QWidget;

namespace adqt::widgets::detail {

void syncTopLevelToolTransientParent(QWidget* toolWindow, QWidget* ownerWindow);

}  // namespace adqt::widgets::detail
