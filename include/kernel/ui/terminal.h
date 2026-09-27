#pragma once

#include <types.h>

namespace ui {

class Terminal {
  public:
    Terminal(ui32 width, ui32 height, ui32 margin_left = 20, ui32 margin_top = 100);

    void draw(ui8 character);

    ui32 get_x() const { return x_; }
    ui32 get_y() const { return y_; }

  private:
    void advance_line_if_needed();

    ui32 width_  = 0;
    ui32 height_ = 0;
    ui32 left_   = 0;
    ui32 top_    = 0;
    ui32 x_      = 0;
    ui32 y_      = 0;
};

} // namespace ui
