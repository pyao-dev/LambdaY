#include <efi.h>
#include <efilib.h>

#include <graphics/basic.h>

namespace {

struct Framebuffer {
    volatile ui8*             address             = nullptr;
    ui64                      size                = 0;
    ui32                      width               = 0;
    ui32                      height              = 0;
    ui32                      pixels_per_scanline = 0;
    EFI_GRAPHICS_PIXEL_FORMAT pixel_format        = PixelFormatMax;
    bool                      initialized         = false;
};

Framebuffer framebuffer;

const EFI_GUID kGraphicsOutputProtocolGuid = {
    0x9042a9de,
    0x23dc,
    0x4a38,
    {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a},
};

bool checked_multiply(ui64 left, ui64 right, ui64& result) {
    if (right != 0 && left > (~static_cast<ui64>(0)) / right) {
        return false;
    }
    result = left * right;
    return true;
}

} // namespace

namespace graphics {

bool initialize(void* system_table) {
    framebuffer = {};

    auto* table = static_cast<EFI_SYSTEM_TABLE*>(system_table);
    if (table == nullptr || table->BootServices == nullptr) {
        return false;
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;
    EFI_STATUS status = table->BootServices->LocateProtocol(const_cast<EFI_GUID*>(&kGraphicsOutputProtocolGuid),
                                                            nullptr, reinterpret_cast<void**>(&gop));
    if (EFI_ERROR(status) || gop == nullptr || gop->Mode == nullptr || gop->Mode->Info == nullptr) {
        return false;
    }

    auto* info = gop->Mode->Info;
    if (gop->Mode->FrameBufferBase == 0 || gop->Mode->FrameBufferSize == 0 || info->HorizontalResolution == 0 ||
        info->VerticalResolution == 0 || info->PixelsPerScanLine < info->HorizontalResolution ||
        info->PixelFormat > PixelBlueGreenRedReserved8BitPerColor) {
        return false;
    }

    ui64 required_pixels = 0;
    ui64 required_bytes  = 0;
    if (!checked_multiply(info->PixelsPerScanLine, info->VerticalResolution, required_pixels) ||
        !checked_multiply(required_pixels, sizeof(ui32), required_bytes) ||
        required_bytes > gop->Mode->FrameBufferSize) {
        return false;
    }

    framebuffer.address             = reinterpret_cast<volatile ui8*>(static_cast<ui64>(gop->Mode->FrameBufferBase));
    framebuffer.size                = gop->Mode->FrameBufferSize;
    framebuffer.width               = info->HorizontalResolution;
    framebuffer.height              = info->VerticalResolution;
    framebuffer.pixels_per_scanline = info->PixelsPerScanLine;
    framebuffer.pixel_format        = info->PixelFormat;
    framebuffer.initialized         = true;
    return true;
}

void put_pixel(ui32 x, ui32 y, ui32 color) {
    if (!framebuffer.initialized || x >= framebuffer.width || y >= framebuffer.height) {
        return;
    }

    ui64 pixel_index = static_cast<ui64>(y) * framebuffer.pixels_per_scanline + x;
    ui64 byte_offset = pixel_index * sizeof(ui32);
    if (byte_offset > framebuffer.size - sizeof(ui32)) {
        return;
    }

    ui8  red   = static_cast<ui8>((color >> 16) & 0xff);
    ui8  green = static_cast<ui8>((color >> 8) & 0xff);
    ui8  blue  = static_cast<ui8>(color & 0xff);
    ui32 pixel = framebuffer.pixel_format == PixelRedGreenBlueReserved8BitPerColor
                     ? (static_cast<ui32>(red) << 16) | (static_cast<ui32>(green) << 8) | blue
                     : (static_cast<ui32>(blue) << 16) | (static_cast<ui32>(green) << 8) | red;

    *reinterpret_cast<volatile ui32*>(framebuffer.address + byte_offset) = pixel;
}

ui32 width() {
    return framebuffer.width;
}

ui32 height() {
    return framebuffer.height;
}

} // namespace graphics
