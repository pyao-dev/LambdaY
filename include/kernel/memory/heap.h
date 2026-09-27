#pragma once

#include <types.h>

namespace memory {

bool initialize_heap();
void* kmalloc(ui64 size);
void kfree(void* address);

} // namespace memory
