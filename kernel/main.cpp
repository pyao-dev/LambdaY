#include <devices/interrupts.h>
#include <devices/keyboard.h>
#include <devices/serial.h>
#include <graphics/basic.h>
#include <graphics/pf/lib.h>
#include <types.h>

namespace {

constexpr ui32 kTextMargin = 20;

char hexadecimal_digit(ui8 value) {
    return value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);
}

class ScancodeDisplay {
  public:
    ScancodeDisplay(ui32 width, ui32 height) : width_(width), height_(height) {
        left_ = width_ > kTextMargin * 2 ? kTextMargin : 0;
        top_  = height_ >= 400 ? 340 : (height_ >= 64 ? 32 : 0);
        x_    = left_;
        y_    = height_ >= top_ + pf::glyph_height * 2 ? top_ + pf::glyph_height : top_;

        pf::draw_text(left_, top_, "PS/2 扫描码：", 0xffffff, 0);
    }

    void draw(ui8 scancode) {
        constexpr ui32 cell_width = pf::ascii_glyph_width * 3;
        const ui32     right      = width_ > left_ ? width_ - left_ : width_;

        if (x_ >= right || right - x_ < cell_width) {
            x_ = left_;
            y_ += pf::glyph_height;
        }
        if (y_ >= height_ || height_ - y_ < pf::glyph_height) {
            x_ = left_;
            y_ = height_ >= top_ + pf::glyph_height * 2 ? top_ + pf::glyph_height : top_;
        }

        const char text[] = {
            hexadecimal_digit(static_cast<ui8>(scancode >> 4)),
            hexadecimal_digit(static_cast<ui8>(scancode & 0x0f)),
            ' ',
            '\0',
        };
        pf::draw_text(x_, y_, text, 0xffffff, 0);
        x_ += cell_width;
    }

  private:
    ui32 width_  = 0;
    ui32 height_ = 0;
    ui32 left_   = 0;
    ui32 top_    = 0;
    ui32 x_      = 0;
    ui32 y_      = 0;
};

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
    ui32 line_length   = screen_width < screen_height ? screen_width : screen_height;

    serial::write("Clear the screen and draw demo lines\n");
    graphics::clear(0);

    for (ui32 offset = 0; offset < line_length; ++offset) {
        graphics::put_pixel(offset, 200, 0xff0000);
        graphics::put_pixel(offset, 250, 0x00ff00);
        graphics::put_pixel(offset, 300, 0x0000ff);
    }

    pf::draw_text(20, 20, "你好！欢迎来到 LambdaY 操作系统！这是：中英混排 Test 测试。", 0xffffff, 0x00);

    interrupts::initialize_idt();
    interrupts::initialize_pic();
    if (!keyboard::initialize()) {
        serial::write("Failed to initialize the PS/2 keyboard.\n");
        pf::draw_text(20, 340, "PS/2 键盘初始化失败！", 0xff0000, 0);
        for (;;) {
            asm volatile("hlt");
        }
    }
    interrupts::enable_keyboard_irq();
    serial::write("PS/2 keyboard initialized; IRQ1 enabled.\n");

    ScancodeDisplay display(screen_width, screen_height);
    interrupts::enable();

    for (;;) {
        ui8 scancode = 0;
        while (keyboard::pop_scancode(scancode)) {
            display.draw(scancode);
        }

        interrupts::disable();
        if (keyboard::has_scancode()) {
            interrupts::enable();
            continue;
        }
        interrupts::enable_and_halt();
    }
}
