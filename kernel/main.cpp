#include <boot_info.h>
#include <devices/interrupts.h>
#include <devices/keyboard.h>
#include <devices/serial.h>
#include <graphics/basic.h>
#include <graphics/pf/lib.h>
#include <memory/heap.h>
#include <memory/paging.h>
#include <memory/physical.h>
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

[[noreturn]] void halt_forever() {
    for (;;) {
        asm volatile("hlt");
    }
}

void write_memory_stats() {
    serial::write("Memory pages: total=");
    serial::write(int2str(memory::total_page_count()));
    serial::write(" free=");
    serial::write(int2str(memory::free_page_count()));
    serial::write(" used=");
    serial::write(int2str(memory::used_page_count()));
    serial::write("\n");
}

bool run_memory_self_test() {
    const ui64 first_page = memory::page_alloc();
    const ui64 pages      = memory::page_alloc(3);
    if (first_page == 0 || pages == 0) {
        serial::write("Memory self-test: page allocation failed.\n");
        return false;
    }

    *reinterpret_cast<volatile ui8*>(first_page) = 0xa5;
    *reinterpret_cast<volatile ui8*>(pages)      = 0x5a;
    if (!memory::page_free(first_page) || !memory::page_free(pages, 3)) {
        serial::write("Memory self-test: page free failed.\n");
        return false;
    }

    auto* small = reinterpret_cast<ui8*>(memory::kmalloc(32));
    auto* large = reinterpret_cast<ui8*>(memory::kmalloc(5000));
    if (small == nullptr || large == nullptr) {
        serial::write("Memory self-test: heap allocation failed.\n");
        return false;
    }
    small[0]    = 0x11;
    large[4999] = 0x22;
    memory::kfree(small);
    memory::kfree(large);

    auto* merged = memory::kmalloc(5000);
    if (merged == nullptr) {
        serial::write("Memory self-test: heap reuse failed.\n");
        return false;
    }
    memory::kfree(merged);
    serial::write("Memory self-test: passed.\n");
    return true;
}

} // namespace

extern "C" __attribute__((ms_abi, noreturn)) void kernel_entry(const BootInfo* boot_info) {
    interrupts::disable();
    serial::initialize();
    serial::write("This is a message from KERNEL.BIN. Welcome!\n");

    if (boot_info == nullptr || boot_info->magic != LAMBDAY_BOOT_INFO_MAGIC ||
        boot_info->version != LAMBDAY_BOOT_INFO_VERSION || boot_info->size < sizeof(BootInfo) ||
        boot_info->memory_map == nullptr || boot_info->memory_map_count == 0) {
        serial::write("Invalid BootInfo received from BootLoader.\n");
        halt_forever();
    }

    serial::write("Memory map entries: ");
    serial::write(int2str(boot_info->memory_map_count));
    serial::write("\n");

    serial::write("Boot: initializing paging.\n");
    if (!memory::initialize_paging(boot_info)) {
        serial::write("Failed to initialize kernel paging.\n");
        halt_forever();
    }
    serial::write("Boot: paging ready.\n");

    serial::write("Boot: initializing physical memory.\n");
    if (!memory::initialize_physical(boot_info)) {
        serial::write("Failed to initialize physical memory allocator.\n");
        halt_forever();
    }
    serial::write("Boot: physical memory ready.\n");

    serial::write("Boot: initializing heap.\n");
    if (!memory::initialize_heap() || !run_memory_self_test()) {
        serial::write("Failed to initialize kernel heap.\n");
        halt_forever();
    }
    serial::write("Boot: heap ready.\n");
    write_memory_stats();

    serial::write("Boot: initializing graphics.\n");
    if (!graphics::initialize(boot_info)) {
        serial::write("Failed to initialize graphics.\n");
        halt_forever();
    }
    serial::write("Boot: graphics ready.\n");

    ui32 screen_width  = graphics::get_width();
    ui32 screen_height = graphics::get_height();

    serial::write("Screen info: size ");
    serial::write(int2str(screen_width));
    serial::write('x');
    serial::write(int2str(screen_height));
    serial::write('\n');

    graphics::clear(0);

    pf::draw_text(kWelcomeTextX, kWelcomeTextY, "你好！欢迎来到 LambdaY 操作系统！这是：中英混排 Test 测试。");

    serial::write("Boot: initializing interrupts.\n");
    interrupts::initialize_idt();
    interrupts::initialize_pic();
    serial::write("Boot: interrupts ready.\n");

    serial::write("Boot: initializing keyboard.\n");
    if (!keyboard::initialize()) {
        serial::write("Failed to initialize the PS/2 keyboard.\n");
        pf::draw_text(kStatusTextX, kStatusTextY, "PS/2 键盘初始化失败！", 0xff0000, 0);
        halt_forever();
    }
    interrupts::enable_keyboard_irq();
    serial::write("PS/2 keyboard initialized; IRQ1 enabled.\n");
    serial::write("Boot: keyboard ready.\n");

    ui::Terminal terminal(screen_width, screen_height, kTerminalMarginX, kTerminalMarginY);
    serial::write("Boot: terminal ready.\n");
    interrupts::enable();
    serial::write("Boot: interrupts enabled.\n");

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
