#include <memory/physical.h>

#include <devices/serial.h>
#include <str.h>

extern "C" char _kernel_start;
extern "C" char _kernel_end;

namespace {

constexpr ui64 kMaxPhysicalAddress = 4ULL * 1024 * 1024 * 1024;
constexpr ui64 kMaxPageCount       = kMaxPhysicalAddress / memory::kPageSize;
constexpr ui64 kBitmapByteCount    = (kMaxPageCount + 7) / 8;

ui8  bitmap[kBitmapByteCount];
ui64 managed_pages = 0;
ui64 free_pages    = 0;
bool initialized   = false;

bool checked_add(ui64 left, ui64 right, ui64& result) {
    if (left > (~static_cast<ui64>(0)) - right) {
        return false;
    }
    result = left + right;
    return true;
}

bool checked_multiply(ui64 left, ui64 right, ui64& result) {
    if (right != 0 && left > (~static_cast<ui64>(0)) / right) {
        return false;
    }
    result = left * right;
    return true;
}

ui64 align_down(ui64 value) {
    return value & ~(memory::kPageSize - 1);
}

ui64 align_up(ui64 value) {
    const ui64 remainder = value & (memory::kPageSize - 1);
    return remainder == 0 ? value : value + memory::kPageSize - remainder;
}

bool bit_is_set(ui64 page) {
    return (bitmap[page / 8] & static_cast<ui8>(1U << (page % 8))) != 0;
}

void set_bit(ui64 page) {
    bitmap[page / 8] |= static_cast<ui8>(1U << (page % 8));
}

void clear_bit(ui64 page) {
    bitmap[page / 8] &= static_cast<ui8>(~(1U << (page % 8)));
}

void mark_range(ui64 address, ui64 size, bool used) {
    ui64 end = 0;
    if (size == 0 || !checked_add(address, size, end)) {
        return;
    }
    const ui64 first_page = align_down(address) / memory::kPageSize;
    const ui64 last_page  = (align_up(end) / memory::kPageSize);
    const ui64 limit      = last_page < managed_pages ? last_page : managed_pages;
    for (ui64 page = first_page; page < limit; ++page) {
        if (used) {
            if (!bit_is_set(page)) {
                set_bit(page);
                if (free_pages != 0) {
                    --free_pages;
                }
            }
        } else if (bit_is_set(page)) {
            clear_bit(page);
            ++free_pages;
        }
    }
}

void reserve_object(const void* address, ui64 size) {
    mark_range(reinterpret_cast<ui64>(address), size, true);
}

void report(const char* message) {
    serial::write("memory: ");
    serial::write(message);
    serial::write("\n");
}

} // namespace

namespace memory {

bool initialize_physical(const BootInfo* boot_info) {
    initialized   = false;
    managed_pages = 0;
    free_pages    = 0;

    for (ui64 index = 0; index < kBitmapByteCount; ++index) {
        bitmap[index] = 0xff;
    }
    if (boot_info == nullptr || boot_info->memory_map == nullptr || boot_info->memory_map_count == 0) {
        report("invalid memory map");
        return false;
    }

    for (ui64 index = 0; index < boot_info->memory_map_count; ++index) {
        const MemoryRegion& region = boot_info->memory_map[index];
        ui64                bytes  = 0;
        ui64                end    = 0;
        if (!checked_multiply(region.page_count, kPageSize, bytes) || !checked_add(region.physical_start, bytes, end)) {
            report("memory map overflow");
            return false;
        }
        if (region.physical_start >= kMaxPhysicalAddress) {
            continue;
        }
        if (end > kMaxPhysicalAddress) {
            end = kMaxPhysicalAddress;
        }
        if (end / kPageSize > managed_pages) {
            managed_pages = end / kPageSize;
        }
    }

    for (ui64 index = 0; index < boot_info->memory_map_count; ++index) {
        const MemoryRegion& region = boot_info->memory_map[index];
        if (region.physical_start >= kMaxPhysicalAddress) {
            continue;
        }
        if (region.type != BootMemoryConventional && region.type != BootMemoryBootServicesCode &&
            region.type != BootMemoryBootServicesData) {
            continue;
        }
        ui64 bytes = 0;
        if (!checked_multiply(region.page_count, kPageSize, bytes)) {
            report("memory map overflow");
            return false;
        }
        mark_range(region.physical_start, bytes, false);
    }

    // Never hand out the null page, live boot metadata, the framebuffer, the
    // current stack, or any part of the linked kernel image.
    mark_range(0, kPageSize, true);
    reserve_object(boot_info, sizeof(BootInfo));
    if (boot_info->memory_map_count <= (~static_cast<ui64>(0)) / sizeof(MemoryRegion)) {
        reserve_object(boot_info->memory_map, boot_info->memory_map_count * sizeof(MemoryRegion));
    }
    reserve_object(reinterpret_cast<const void*>(boot_info->framebuffer.address), boot_info->framebuffer.size);
    ui8 stack_marker = 0;
    reserve_object(&stack_marker, kPageSize);
    reserve_object(&_kernel_start, static_cast<ui64>(&_kernel_end - &_kernel_start));

    initialized = true;
    report("physical allocator initialized");
    return true;
}

ui64 page_alloc(ui64 page_count) {
    if (!initialized || page_count == 0 || page_count > managed_pages) {
        return 0;
    }

    for (ui64 first = 1; first + page_count <= managed_pages; ++first) {
        bool available = true;
        for (ui64 offset = 0; offset < page_count; ++offset) {
            if (bit_is_set(first + offset)) {
                available = false;
                first += offset;
                break;
            }
        }
        if (!available) {
            continue;
        }
        for (ui64 offset = 0; offset < page_count; ++offset) {
            set_bit(first + offset);
        }
        free_pages -= page_count;
        return first * kPageSize;
    }

    report("page allocation failed");
    return 0;
}

bool page_free(ui64 physical_address, ui64 page_count) {
    if (!initialized || page_count == 0 || physical_address == 0 || physical_address % kPageSize != 0) {
        report("invalid page free");
        return false;
    }
    const ui64 first = physical_address / kPageSize;
    if (first >= managed_pages || page_count > managed_pages - first) {
        report("page free out of range");
        return false;
    }
    for (ui64 offset = 0; offset < page_count; ++offset) {
        if (!bit_is_set(first + offset)) {
            report("double page free");
            return false;
        }
    }
    for (ui64 offset = 0; offset < page_count; ++offset) {
        clear_bit(first + offset);
    }
    free_pages += page_count;
    return true;
}

ui64 total_page_count() {
    return managed_pages;
}

ui64 free_page_count() {
    return free_pages;
}

ui64 used_page_count() {
    return managed_pages >= free_pages ? managed_pages - free_pages : 0;
}

} // namespace memory
