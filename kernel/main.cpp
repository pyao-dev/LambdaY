#include <devices/interrupts.h>
#include <devices/keyboard.h>
#include <devices/serial.h>
#include <graphics/basic.h>
#include <graphics/pf/lib.h>
#include <str.h>
#include <types.h>
#include <ui/terminal.h>

namespace {

constexpr ui32 kWelcomeTextX    = 20;
constexpr ui32 kWelcomeTextY    = 20;
constexpr ui32 kStatusTextX     = 20;
constexpr ui32 kStatusTextY     = 60;
constexpr ui32 kTerminalMarginX = 20;
constexpr ui32 kTerminalMarginY = 100;

} // namespace

extern "C" __attribute__((ms_abi, noreturn)) void kernel_entry(void*, void* system_table) {
    interrupts::disable();
    serial::initialize();
    serial::write("This is a message from KERNEL.BIN. Welcome!\n");

    if (!graphics::initialize(system_table)) {
        serial::write("Failed to initialize graphics.\n");
        asm volatile("hlt");
    }

    ui32 screen_width  = graphics::get_width();
    ui32 screen_height = graphics::get_height();

    serial::write("Screen info: size ");
    serial::write(int2str(screen_width));
    serial::write('x');
    serial::write(int2str(screen_height));

    serial::write("\nClear the screen and draw demo lines\n");
    graphics::clear(0);

    /*
    for (ui32 offset = 0; offset < line_length; ++offset) {
        graphics::put_pixel(offset, 200, 0xff0000);
        graphics::put_pixel(offset, 250, 0x00ff00);
        graphics::put_pixel(offset, 300, 0x0000ff);
    }
    */

    pf::draw_text(kWelcomeTextX, kWelcomeTextY, "你好！欢迎来到 LambdaY 操作系统！这是：中英混排 Test 测试。");

    interrupts::initialize_idt();
    interrupts::initialize_pic();
    if (!keyboard::initialize()) {
        serial::write("Failed to initialize the PS/2 keyboard.\n");
        pf::draw_text(kStatusTextX, kStatusTextY, "PS/2 键盘初始化失败！", 0xff0000, 0);
        for (;;) {
            asm volatile("hlt");
        }
    }
    interrupts::enable_keyboard_irq();
    serial::write("PS/2 keyboard initialized; IRQ1 enabled.\n");

    ui::Terminal terminal(screen_width, screen_height, kTerminalMarginX, kTerminalMarginY);
    interrupts::enable();

    pf::draw_text(kStatusTextX, kStatusTextY, "PS/2键盘初始化成功，现在你可以键入一些内容");

    for (;;) {
        ui8 character = 0;
        while (keyboard::pop_ascii(character)) {
            terminal.draw(character);
            serial::write(static_cast<char>(character));
        }

        interrupts::disable();
        if (keyboard::has_ascii()) {
            interrupts::enable();
            continue;
        }
        interrupts::enable_and_halt();
    }
}
