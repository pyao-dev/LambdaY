#pragma once

#include <boot_info.h>

namespace memory {

bool initialize_paging(const BootInfo* boot_info);

} // namespace memory
