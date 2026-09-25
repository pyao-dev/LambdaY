#include <devices/io.h>
#include <devices/serial.h>
#include <types.h>

namespace serial {

void initialize() {
    io::outb(kCom1 + 1, 0x00);
    io::outb(kCom1 + 3, 0x80);
    io::outb(kCom1 + 0, 0x03);
    io::outb(kCom1 + 1, 0x00);
    io::outb(kCom1 + 3, 0x03);
    io::outb(kCom1 + 2, 0xc7);
    io::outb(kCom1 + 4, 0x0b);
}

void write(char character) {
    while ((io::inb(kCom1 + 5) & 0x20) == 0)
        ;
    io::outb(kCom1, static_cast<ui8>(character));
}

void write(const char* message) {
    while (*message != '\0')
        write(*message++);
}

} // namespace serial
