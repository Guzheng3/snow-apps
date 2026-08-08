#pragma once

#include "snow_draw_engine.h"

#include <cstdint>

class QWidget;

namespace snow_canvas_interaction {

class Controller final {
  public:
    bool isEnabled() const;
    std::uint32_t capturedPointerId() const;

    void setEnabled(QWidget& widget, bool enabled);
    void clearTransientState(QWidget& widget);
    void applyOutput(QWidget& widget, const SnowInteractionOutput& output);

  private:
    bool m_enabled = true;
    std::uint32_t m_capturedPointerId = 0;
};

} // namespace snow_canvas_interaction
