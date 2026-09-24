#include <devices/serial.h>
#include <types.h>

extern "C" __attribute__((ms_abi, noreturn)) void kernel_entry(void*, void*) {
    serial::initialize();
    serial::write("This is a message from KERNEL.BIN. Welcome!\r\n");

    for (;;) {
        asm volatile("hlt");
    }
}
