#include <devices/interrupts.h>
#include <devices/keyboard.h>
#include <devices/serial.h>
#include <graphics/basic.h>
#include <graphics/pf/lib.h>
#include <str.h>
#include <types.h>

namespace {

constexpr ui32 kTextMargin = 20;

class Terminal {
  public:
    Terminal(ui32 width, ui32 height) : width_(width), height_(height) {
        left_ = 20;
        top_  = 100;
        x_    = left_;
        y_    = top_;
    }

    void draw(ui8 character) {
        if (character == '\b') {
            if (x_ > left_) {
                x_ -= pf::ascii_glyph_width;
                pf::draw_ascii(x_, y_, ' ', 0xffffff, 0);
            }
            return;
        }
        if (character == '\r') {
            x_ = left_;
            return;
        }
        if (character == '\n') {
            x_ = left_;
            y_ += pf::glyph_height;
            advance_line_if_needed();
            return;
        }
        if (character < 0x20 || character > 0x7e) {
            return;
        }

        if (x_ > width_ || width_ - x_ < pf::ascii_glyph_width) {
            x_ = left_;
            y_ += pf::glyph_height;
            advance_line_if_needed();
        }
        pf::draw_ascii(x_, y_, character, 0xffffff, 0);
        x_ += pf::ascii_glyph_width;
    }

  private:
    void advance_line_if_needed() {
        if (height_ == 0 || y_ + pf::glyph_height <= height_) {
            return;
        }
        graphics::clear(0);
        y_ = top_;
    }

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

    pf::draw_text(20, 20, "你好！欢迎来到 LambdaY 操作系统！这是：中英混排 Test 测试。");

    interrupts::initialize_idt();
    interrupts::initialize_pic();
    if (!keyboard::initialize()) {
        serial::write("Failed to initialize the PS/2 keyboard.\n");
        pf::draw_text(20, 60, "PS/2 键盘初始化失败！", 0xff0000, 0);
        for (;;) {
            asm volatile("hlt");
        }
    }
    interrupts::enable_keyboard_irq();
    serial::write("PS/2 keyboard initialized; IRQ1 enabled.\n");

    Terminal terminal(screen_width, screen_height);
    interrupts::enable();

    pf::draw_text(20, 60, "PS/2键盘初始化成功，现在你可以键入一些内容");

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
