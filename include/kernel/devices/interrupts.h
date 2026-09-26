#pragma once

namespace interrupts {

void disable();
void enable();
void enable_and_halt();

void initialize_idt();
void initialize_pic();
void enable_keyboard_irq();
void send_master_eoi();

} // namespace interrupts
