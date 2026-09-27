#include <memory/paging.h>
#include <memory/physical.h>

#include <types.h>

namespace {

constexpr ui64 kTwoMiB          = 2 * 1024 * 1024;
constexpr ui64 kFourGiB         = 4ULL * 1024 * 1024 * 1024;
constexpr ui64 kPageTableCount  = 4;
constexpr ui64 kEntriesPerTable = 512;

alignas(4096) ui64 pml4[kEntriesPerTable];
alignas(4096) ui64 pdpt[kEntriesPerTable];
alignas(4096) ui64 page_directories[kPageTableCount][kEntriesPerTable];

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

ui64 align_up(ui64 value, ui64 alignment) {
    const ui64 remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

} // namespace

namespace memory {

bool initialize_paging(const BootInfo* boot_info) {
    if (boot_info == nullptr || boot_info->memory_map == nullptr || boot_info->memory_map_count == 0) {
        return false;
    }

    ui64 max_end = 0;
    for (ui64 index = 0; index < boot_info->memory_map_count; ++index) {
        const MemoryRegion& region = boot_info->memory_map[index];
        if (region.physical_start >= kFourGiB) {
            continue;
        }
        ui64 bytes = 0;
        ui64 end   = 0;
        if (!checked_multiply(region.page_count, kPageSize, bytes) || !checked_add(region.physical_start, bytes, end)) {
            return false;
        }
        if (end > kFourGiB) {
            end = kFourGiB;
        }
        if (end > max_end) {
            max_end = end;
        }
    }

    ui64 framebuffer_end = 0;
    if (!checked_add(boot_info->framebuffer.address, boot_info->framebuffer.size, framebuffer_end)) {
        return false;
    }
    if (boot_info->framebuffer.address >= kFourGiB || framebuffer_end > kFourGiB) {
        return false;
    }
    if (framebuffer_end > max_end) {
        max_end = framebuffer_end;
    }
    if (max_end == 0 || max_end > kFourGiB) {
        return false;
    }

    const ui64 mapped_end = align_up(max_end, kTwoMiB);
    if (mapped_end > kFourGiB) {
        return false;
    }

    for (ui64 index = 0; index < kEntriesPerTable; ++index) {
        pml4[index] = 0;
        pdpt[index] = 0;
    }
    for (ui64 directory = 0; directory < kPageTableCount; ++directory) {
        for (ui64 entry = 0; entry < kEntriesPerTable; ++entry) {
            page_directories[directory][entry] = 0;
        }
    }

    pml4[0] = reinterpret_cast<ui64>(&pdpt[0]) | 0x003;
    for (ui64 directory = 0; directory < kPageTableCount; ++directory) {
        pdpt[directory] = reinterpret_cast<ui64>(&page_directories[directory][0]) | 0x003;
    }

    const ui64 page_count = mapped_end / kTwoMiB;
    for (ui64 page = 0; page < page_count; ++page) {
        const ui64 directory               = page / kEntriesPerTable;
        const ui64 entry                   = page % kEntriesPerTable;
        page_directories[directory][entry] = page * kTwoMiB | 0x083; // present, writable, 2 MiB page
    }

    asm volatile("mov %0, %%cr3" : : "r"(reinterpret_cast<ui64>(&pml4[0])) : "memory");
    return true;
}

} // namespace memory
