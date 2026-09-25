#include <graphics/basic.h>

// 玄学问题
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#include <graphics/pf/fontdata/asc16.h>
#include <graphics/pf/fontdata/gb2312.h>
#include <graphics/pf/fontdata/hzk16.h>
#pragma GCC diagnostic pop

#include <graphics/pf/lib.h>

namespace pf {

namespace {

constexpr ui8  first_code       = 0xA1;
constexpr ui8  last_code        = 0xFE;
constexpr ui32 bytes_per_glyph  = 32;
constexpr ui32 glyphs_per_plane = 94;

struct Utf8CodePoint {
    ui32 value;
    ui32 length;
};

bool decode_utf8(const char* text, Utf8CodePoint& code_point) {
    const ui8 first = static_cast<ui8>(text[0]);
    if (first < 0x80) {
        code_point = {first, 1};
        return true;
    }

    ui32 length  = 0;
    ui32 value   = 0;
    ui32 minimum = 0;
    if ((first & 0xE0) == 0xC0) {
        length  = 2;
        value   = first & 0x1F;
        minimum = 0x80;
    } else if ((first & 0xF0) == 0xE0) {
        length  = 3;
        value   = first & 0x0F;
        minimum = 0x800;
    } else if ((first & 0xF8) == 0xF0) {
        length  = 4;
        value   = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }

    for (ui32 index = 1; index < length; ++index) {
        const ui8 byte = static_cast<ui8>(text[index]);
        if (byte == 0 || (byte & 0xC0) != 0x80) {
            return false;
        }
        value = (value << 6) | (byte & 0x3F);
    }

    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }

    code_point = {value, length};
    return true;
}

bool unicode_to_gb2312(ui32 value, ui8& first, ui8& second) {
    ui32 left  = 0;
    ui32 right = gb2312_mapping_count;
    while (left < right) {
        const ui32           middle  = left + (right - left) / 2;
        const Gb2312Mapping& mapping = gb2312_mappings[middle];
        if (mapping.unicode < value) {
            left = middle + 1;
        } else if (mapping.unicode > value) {
            right = middle;
        } else {
            first  = static_cast<ui8>(mapping.code >> 8);
            second = static_cast<ui8>(mapping.code & 0xff);
            return true;
        }
    }
    return false;
}

bool valid_code(ui8 first, ui8 second) {
    return first >= first_code && first <= last_code && second >= first_code && second <= last_code;
}

bool glyph_offset(ui8 first, ui8 second, ui64& offset) {
    if (!valid_code(first, second)) {
        return false;
    }

    const ui64 index =
        static_cast<ui64>(first - first_code) * glyphs_per_plane + static_cast<ui64>(second - first_code);
    offset = index * bytes_per_glyph;
    return offset <= sizeof(HZK16) && sizeof(HZK16) - offset >= bytes_per_glyph;
}

void draw_row(ui32 x, ui32 y, ui16 row, ui32 foreground, ui32 background) {
    for (ui32 column = 0; column < glyph_width; ++column) {
        const ui32 color = (row & (static_cast<ui16>(1U << (15U - column)))) ? foreground : background;
        graphics::put_pixel(x + column, y, color);
    }
}

void draw_ascii_row(ui32 x, ui32 y, ui8 row, ui32 foreground, ui32 background) {
    for (ui32 column = 0; column < ascii_glyph_width; ++column) {
        const ui32 color = (row & (static_cast<ui8>(1U << (7U - column)))) ? foreground : background;
        graphics::put_pixel(x + column, y, color);
    }
}

} // namespace

bool draw_ascii(ui32 x, ui32 y, ui8 character, ui32 foreground, ui32 background) {
    const ui64 offset = static_cast<ui64>(character) * glyph_height;
    if (offset > sizeof(ASC16) || sizeof(ASC16) - offset < glyph_height) {
        return false;
    }

    for (ui32 row = 0; row < glyph_height; ++row) {
        draw_ascii_row(x, y + row, ASC16[offset + row], foreground, background);
    }

    return true;
}

bool draw_gb2312(ui32 x, ui32 y, ui8 first, ui8 second, ui32 foreground, ui32 background) {
    ui64 offset = 0;
    if (!glyph_offset(first, second, offset)) {
        return false;
    }

    for (ui32 row = 0; row < glyph_height; ++row) {
        const ui64 row_offset = offset + static_cast<ui64>(row) * 2;
        const ui16 bits =
            static_cast<ui16>(static_cast<ui8>(HZK16[row_offset])) << 8 | static_cast<ui8>(HZK16[row_offset + 1]);
        draw_row(x, y + row, bits, foreground, background);
    }

    return true;
}

bool draw_text(ui32 x, ui32 y, const char* text, ui32 foreground, ui32 background) {
    if (text == nullptr) {
        return false;
    }

    const ui32 origin_x = x;
    bool       valid    = true;
    while (*text != '\0') {
        Utf8CodePoint code_point = {};
        if (!decode_utf8(text, code_point)) {
            return false;
        }

        if (code_point.value == '\r') {
            text += code_point.length;
            continue;
        }
        if (code_point.value == '\n') {
            x = origin_x;
            y += glyph_height;
            text += code_point.length;
            continue;
        }

        if (code_point.value < 0x80) {
            if (graphics::get_width() != 0 && x > graphics::get_width() - 1 - ascii_glyph_width) {
                x = origin_x;
                y += glyph_height;
            }
            if (graphics::get_height() != 0 && y > graphics::get_height() - glyph_height) {
                return false;
            }
            if (!draw_ascii(x, y, static_cast<ui8>(code_point.value), foreground, background)) {
                return false;
            }
            x += ascii_glyph_width;
            text += code_point.length;
            continue;
        }

        ui8 first  = 0;
        ui8 second = 0;
        if (!unicode_to_gb2312(code_point.value, first, second)) {
            return false;
        }

        if (graphics::get_width() != 0 && x > graphics::get_width() - 1 - glyph_width) {
            x = origin_x;
            y += glyph_height;
        }
        if (graphics::get_height() != 0 && y > graphics::get_height() - glyph_height) {
            return false;
        }

        if (!draw_gb2312(x, y, first, second, foreground, background)) {
            return false;
        }
        x += glyph_width;
        text += code_point.length;
    }

    return valid;
}

} // namespace pf
