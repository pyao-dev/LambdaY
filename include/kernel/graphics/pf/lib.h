#pragma once

#include <types.h>

namespace pf {

constexpr ui32 glyph_width       = 16;
constexpr ui32 glyph_height      = 16;
constexpr ui32 ascii_glyph_width = 8;

bool draw_ascii(ui32 x, ui32 y, ui8 character, ui32 foreground = 0xffffff, ui32 background = 0);
bool draw_gb2312(ui32 x, ui32 y, ui8 first, ui8 second, ui32 foreground = 0xffffff, ui32 background = 0);
bool draw_text(ui32 x, ui32 y, const char* text, ui32 foreground = 0xffffff, ui32 background = 0);

} // namespace pf
