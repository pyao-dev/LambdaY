#pragma once

#include <boot_info.h>
#include <types.h>

namespace graphics {

bool initialize(const BootInfo* boot_info);
void clear(ui32 color);
void put_pixel(ui32 x, ui32 y, ui32 color);

ui32 get_width();
ui32 get_height();

} // namespace graphics
