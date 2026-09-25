#include <devices/serial.h>
#include <graphics/basic.h>
#include <types.h>

extern "C" __attribute__((ms_abi, noreturn)) void kernel_entry(void*, void* system_table) {
    serial::initialize();
    serial::write("This is a message from KERNEL.BIN. Welcome!\n");

    if (!graphics::initialize(system_table)) {
        serial::write("Failed to initialize graphics.\n");
        asm volatile("hlt");
    }

    ui32 screen_width  = graphics::width();
    ui32 screen_height = graphics::height();
    ui32 line_length   = screen_width < screen_height ? screen_width : screen_height;

    serial::write("Clear the screen and draw demo lines\n");
    for (ui32 x = 0; x < screen_width; x++)
        for (ui32 y = 0; y < screen_height; y++)
            graphics::put_pixel(x, y, 0);

    for (ui32 offset = 0; offset < line_length; ++offset) {
        graphics::put_pixel(offset, 200, 0xff0000);
        graphics::put_pixel(offset, 250, 0x00ff00);
        graphics::put_pixel(offset, 300, 0x0000ff);
    }

    for (;;) {
        asm volatile("hlt");
    }
}
