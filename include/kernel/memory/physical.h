#pragma once

#include <boot_info.h>
#include <types.h>

namespace memory {

constexpr ui64 kPageSize = 4096;

bool initialize_physical(const BootInfo* boot_info);
ui64 page_alloc(ui64 page_count = 1);
bool page_free(ui64 physical_address, ui64 page_count = 1);

ui64 total_page_count();
ui64 free_page_count();
ui64 used_page_count();

} // namespace memory
