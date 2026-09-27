#include <graphics/basic.h>

namespace {

struct Framebuffer {
    volatile ui8* address             = nullptr;
    ui64          size                = 0;
    ui32          width               = 0;
    ui32          height              = 0;
    ui32          pixels_per_scanline = 0;
    ui32          pixel_format        = BootPixelFormatRgb;
    bool          initialized         = false;
};

Framebuffer framebuffer;

bool checked_multiply(ui64 left, ui64 right, ui64& result) {
    if (right != 0 && left > (~static_cast<ui64>(0)) / right) {
        return false;
    }
    result = left * right;
    return true;
}

ui32 encode_pixel(ui32 color) {
    const ui8 red   = static_cast<ui8>((color >> 16) & 0xff);
    const ui8 green = static_cast<ui8>((color >> 8) & 0xff);
    const ui8 blue  = static_cast<ui8>(color & 0xff);
    return framebuffer.pixel_format == BootPixelFormatRgb
               ? (static_cast<ui32>(red) << 16) | (static_cast<ui32>(green) << 8) | blue
               : (static_cast<ui32>(blue) << 16) | (static_cast<ui32>(green) << 8) | red;
}

} // namespace

namespace graphics {

bool initialize(const BootInfo* boot_info) {
    framebuffer = {};

    if (boot_info == nullptr || boot_info->magic != LAMBDAY_BOOT_INFO_MAGIC ||
        boot_info->version != LAMBDAY_BOOT_INFO_VERSION || boot_info->size < sizeof(BootInfo)) {
        return false;
    }

    const FramebufferInfo& info = boot_info->framebuffer;
    if (info.address == 0 || info.size == 0 || info.width == 0 || info.height == 0 ||
        info.pixels_per_scanline < info.width || info.pixel_format > BootPixelFormatBgr) {
        return false;
    }

    ui64 required_pixels = 0;
    ui64 required_bytes  = 0;
    if (!checked_multiply(info.pixels_per_scanline, info.height, required_pixels) ||
        !checked_multiply(required_pixels, sizeof(ui32), required_bytes) || required_bytes > info.size) {
        return false;
    }

    framebuffer.address             = reinterpret_cast<volatile ui8*>(info.address);
    framebuffer.size                = info.size;
    framebuffer.width               = info.width;
    framebuffer.height              = info.height;
    framebuffer.pixels_per_scanline = info.pixels_per_scanline;
    framebuffer.pixel_format        = info.pixel_format;
    framebuffer.initialized         = true;
    return true;
}

void clear(ui32 color) {
    if (!framebuffer.initialized) {
        return;
    }

    const ui32 pixel = encode_pixel(color);

    // 优化：使用批量填充代替逐像素写入
    // 先填充第一行
    volatile ui32* first_row = reinterpret_cast<volatile ui32*>(framebuffer.address);
    for (ui32 x = 0; x < framebuffer.width; ++x) {
        first_row[x] = pixel;
    }

    // 如果每行像素数等于宽度，可以一次性填充整个framebuffer
    if (framebuffer.pixels_per_scanline == framebuffer.width) {
        const ui64     total_pixels = static_cast<ui64>(framebuffer.width) * framebuffer.height;
        volatile ui32* all_pixels   = reinterpret_cast<volatile ui32*>(framebuffer.address);
        for (ui64 i = framebuffer.width; i < total_pixels; ++i) {
            all_pixels[i] = pixel;
        }
    } else {
        // 否则逐行复制第一行的数据
        for (ui32 y = 1; y < framebuffer.height; ++y) {
            volatile ui32* row = reinterpret_cast<volatile ui32*>(
                framebuffer.address + static_cast<ui64>(y) * framebuffer.pixels_per_scanline * sizeof(ui32));
            for (ui32 x = 0; x < framebuffer.width; ++x) {
                row[x] = pixel;
            }
        }
    }
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

    *reinterpret_cast<volatile ui32*>(framebuffer.address + byte_offset) = encode_pixel(color);
}

ui32 get_width() {
    return framebuffer.width;
}

ui32 get_height() {
    return framebuffer.height;
}

} // namespace graphics
