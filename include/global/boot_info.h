#pragma once

#include <types.h>

#define LAMBDAY_BOOT_INFO_MAGIC   0x4c594249U
#define LAMBDAY_BOOT_INFO_VERSION 1U

enum {
    BootPixelFormatRgb = 0,
    BootPixelFormatBgr = 1,
};

// EFI memory types copied by the bootloader. Keep these values independent of
// gnu-efi so the kernel can inspect the memory map after ExitBootServices.
enum {
    BootMemoryReserved            = 0,
    BootMemoryLoaderCode         = 1,
    BootMemoryLoaderData         = 2,
    BootMemoryBootServicesCode   = 3,
    BootMemoryBootServicesData   = 4,
    BootMemoryRuntimeCode        = 5,
    BootMemoryRuntimeData        = 6,
    BootMemoryConventional       = 7,
    BootMemoryUnusable           = 8,
    BootMemoryAcpiReclaim        = 9,
    BootMemoryAcpiNvs            = 10,
    BootMemoryMmio               = 11,
    BootMemoryMmioPortSpace     = 12,
    BootMemoryPalCode            = 13,
    BootMemoryPersistent         = 14,
};

typedef struct {
    ui64 address;
    ui64 size;
    ui32 width;
    ui32 height;
    ui32 pixels_per_scanline;
    ui32 pixel_format;
} FramebufferInfo;

typedef struct {
    ui32 type;
    ui32 reserved;
    ui64 physical_start;
    ui64 virtual_start;
    ui64 page_count;
    ui64 attributes;
} MemoryRegion;

typedef struct {
    ui32 magic;
    ui32 version;
    ui64 size;
    FramebufferInfo framebuffer;
    MemoryRegion* memory_map;
    ui64 memory_map_count;
} BootInfo;
