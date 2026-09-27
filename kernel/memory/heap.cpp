#include <memory/heap.h>

#include <devices/serial.h>
#include <memory/physical.h>

namespace {

constexpr ui64 kAlignment    = 16;
constexpr ui64 kInitialPages = 16;
constexpr ui64 kMaxUi64      = ~static_cast<ui64>(0);

struct HeapBlock {
    ui64       size;
    bool       free;
    HeapBlock* previous;
    HeapBlock* next;
};

struct HeapSegment {
    HeapSegment* next;
    ui64         page_count;
    HeapBlock*   first;
};

HeapSegment* segments    = nullptr;
bool         initialized = false;

ui64 align_up(ui64 value) {
    const ui64 remainder = value % kAlignment;
    return remainder == 0 ? value : value + kAlignment - remainder;
}

void report(const char* message) {
    serial::write("memory: ");
    serial::write(message);
    serial::write("\n");
}

HeapBlock* find_block(ui64 required) {
    for (HeapSegment* segment = segments; segment != nullptr; segment = segment->next) {
        for (HeapBlock* block = segment->first; block != nullptr; block = block->next) {
            if (block->free && block->size >= required) {
                return block;
            }
        }
    }
    return nullptr;
}

HeapBlock* add_segment(ui64 required) {
    const ui64 overhead = sizeof(HeapSegment) + sizeof(HeapBlock) + kAlignment;
    if (required > kMaxUi64 - overhead) {
        return nullptr;
    }
    ui64 bytes = required + overhead;
    ui64 pages = (bytes + memory::kPageSize - 1) / memory::kPageSize;
    if (pages < kInitialPages) {
        pages = kInitialPages;
    }

    const ui64 physical = memory::page_alloc(pages);
    if (physical == 0) {
        return nullptr;
    }

    auto*      segment       = reinterpret_cast<HeapSegment*>(physical);
    auto       block_address = (physical + sizeof(HeapSegment) + (kAlignment - 1)) & ~(kAlignment - 1);
    auto*      block         = reinterpret_cast<HeapBlock*>(block_address);
    const ui64 segment_bytes = pages * memory::kPageSize;
    const ui64 used_bytes    = (block_address - physical) + sizeof(HeapBlock);
    if (used_bytes >= segment_bytes || segment_bytes - used_bytes < required) {
        memory::page_free(physical, pages);
        return nullptr;
    }

    segment->next       = segments;
    segment->page_count = pages;
    segment->first      = block;
    segments            = segment;

    block->size     = segment_bytes - used_bytes;
    block->free     = true;
    block->previous = nullptr;
    block->next     = nullptr;
    return block;
}

void split_block(HeapBlock* block, ui64 required) {
    if (block->size < required + sizeof(HeapBlock) + kAlignment) {
        return;
    }
    auto* next     = reinterpret_cast<HeapBlock*>(reinterpret_cast<ui8*>(block + 1) + required);
    next->size     = block->size - required - sizeof(HeapBlock);
    next->free     = true;
    next->previous = block;
    next->next     = block->next;
    if (next->next != nullptr) {
        next->next->previous = next;
    }
    block->next = next;
    block->size = required;
}

void merge_with_next(HeapBlock* block) {
    HeapBlock* next = block->next;
    if (next == nullptr || !next->free) {
        return;
    }
    block->size += sizeof(HeapBlock) + next->size;
    block->next = next->next;
    if (block->next != nullptr) {
        block->next->previous = block;
    }
}

} // namespace

namespace memory {

bool initialize_heap() {
    segments    = nullptr;
    initialized = true;
    return true;
}

void* kmalloc(ui64 size) {
    if (!initialized || size == 0) {
        return nullptr;
    }
    const ui64 required = align_up(size);
    if (required < size) {
        report("heap allocation overflow");
        return nullptr;
    }

    HeapBlock* block = find_block(required);
    if (block == nullptr) {
        block = add_segment(required);
    }
    if (block == nullptr) {
        report("heap allocation failed");
        return nullptr;
    }
    split_block(block, required);
    block->free = false;
    return block + 1;
}

void kfree(void* address) {
    if (!initialized || address == nullptr) {
        return;
    }

    for (HeapSegment* segment = segments; segment != nullptr; segment = segment->next) {
        for (HeapBlock* block = segment->first; block != nullptr; block = block->next) {
            if (block + 1 != address) {
                continue;
            }
            if (block->free) {
                report("double heap free");
                return;
            }
            block->free = true;
            if (block->previous != nullptr && block->previous->free) {
                block = block->previous;
                merge_with_next(block);
            }
            merge_with_next(block);
            return;
        }
    }
    report("invalid heap free");
}

} // namespace memory
