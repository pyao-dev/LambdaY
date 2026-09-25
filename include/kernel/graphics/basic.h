#pragma once

#include <types.h>

namespace graphics {

bool initialize(void* system_table);
void put_pixel(ui32 x, ui32 y, ui32 color);

ui32 width();
ui32 height();

} // namespace graphics
