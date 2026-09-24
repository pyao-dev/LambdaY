#pragma once

#include <types.h>

namespace io {
static inline void outb(ui16 port, ui8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline ui8 inb(ui16 port) {
    ui8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
} // namespace io
