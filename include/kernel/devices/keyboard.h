#pragma once

#include <types.h>

namespace keyboard {

bool initialize();
bool pop_scancode(ui8& scancode);
bool has_scancode();
bool pop_ascii(ui8& character);
bool has_ascii();

} // namespace keyboard

extern "C" void keyboard_handle_irq();
