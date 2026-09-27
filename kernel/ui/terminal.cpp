#include <graphics/basic.h>
#include <graphics/pf/lib.h>
#include <ui/terminal.h>

namespace ui {

Terminal::Terminal(ui32 width, ui32 height, ui32 margin_left, ui32 margin_top)
    : width_(width), height_(height), left_(margin_left), top_(margin_top), x_(margin_left), y_(margin_top) {}

void Terminal::draw(ui8 character) {
    if (character == '\b') {
        if (x_ > left_) {
            x_ -= pf::ascii_glyph_width;
            pf::draw_ascii(x_, y_, ' ', 0xffffff, 0);
        }
        return;
    }
    if (character == '\r') {
        x_ = left_;
        return;
    }
    if (character == '\n') {
        x_ = left_;
        y_ += pf::glyph_height;
        advance_line_if_needed();
        return;
    }
    if (character < 0x20 || character > 0x7e) {
        return;
    }

    if (x_ > width_ || width_ - x_ < pf::ascii_glyph_width) {
        x_ = left_;
        y_ += pf::glyph_height;
        advance_line_if_needed();
    }
    pf::draw_ascii(x_, y_, character, 0xffffff, 0);
    x_ += pf::ascii_glyph_width;
}

void Terminal::advance_line_if_needed() {
    if (height_ == 0 || y_ + pf::glyph_height <= height_) {
        return;
    }
    graphics::clear(0);
    y_ = top_;
}

} // namespace ui
