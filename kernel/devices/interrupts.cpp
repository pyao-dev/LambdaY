#include <devices/interrupts.h>
#include <devices/io.h>
#include <devices/serial.h>
#include <str.h>
#include <types.h>

extern "C" void interrupt_default_entry();
extern "C" void interrupt_keyboard_entry();
extern "C" void interrupt_invalid_opcode_entry();
extern "C" void interrupt_general_protection_entry();
extern "C" void interrupt_page_fault_entry();

namespace {

constexpr ui16 kPicMasterCommand = 0x20;
constexpr ui16 kPicMasterData    = 0x21;
constexpr ui16 kPicSlaveCommand  = 0xa0;
constexpr ui16 kPicSlaveData     = 0xa1;

constexpr ui8 kPicInitialize       = 0x11;
constexpr ui8 kPic8086Mode         = 0x01;
constexpr ui8 kPicMasterOffset     = 0x20;
constexpr ui8 kPicSlaveOffset      = 0x28;
constexpr ui8 kPicMasterHasSlaveAt = 0x04;
constexpr ui8 kPicSlaveIdentity    = 0x02;
constexpr ui8 kPicEndOfInterrupt   = 0x20;

constexpr ui8 kPresentInterruptGate = 0x8e;
constexpr ui8 kKeyboardVector       = 0x21;

constexpr ui32 kApicBaseMsr           = 0x1b;
constexpr ui32 kApicEnabled           = 1U << 11;
constexpr ui32 kX2ApicEnabled         = 1U << 10;
constexpr ui32 kApicRegisterMask      = 0xfffff000;
constexpr ui32 kApicTimerLvtOffset    = 0x320;
constexpr ui32 kApicInitialCount      = 0x380;
constexpr ui32 kApicLvtMasked         = 1U << 16;
constexpr ui32 kX2ApicTimerLvtMsr     = 0x832;
constexpr ui32 kX2ApicInitialCountMsr = 0x838;

struct __attribute__((packed)) IdtEntry {
    ui16 offset_low;
    ui16 selector;
    ui8  ist;
    ui8  attributes;
    ui16 offset_middle;
    ui32 offset_high;
    ui32 reserved;
};

struct __attribute__((packed)) Idtr {
    ui16 limit;
    ui64 base;
};

static_assert(sizeof(IdtEntry) == 16, "An x86_64 IDT entry must be 16 bytes");
static_assert(sizeof(Idtr) == 10, "An x86_64 IDTR must be 10 bytes");

alignas(16) IdtEntry idt[256] = {};

void io_wait() {
    io::outb(0x80, 0);
}

ui64 read_msr(ui32 index) {
    ui32 low  = 0;
    ui32 high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(index));
    return (static_cast<ui64>(high) << 32) | low;
}

void write_msr(ui32 index, ui64 value) {
    asm volatile("wrmsr" : : "c"(index), "a"(static_cast<ui32>(value)), "d"(static_cast<ui32>(value >> 32)) : "memory");
}

void stop_firmware_apic_timer() {
    const ui64 apic_base = read_msr(kApicBaseMsr);
    if ((apic_base & kApicEnabled) == 0) {
        return;
    }

    if ((apic_base & kX2ApicEnabled) != 0) {
        write_msr(kX2ApicTimerLvtMsr, read_msr(kX2ApicTimerLvtMsr) | kApicLvtMasked);
        write_msr(kX2ApicInitialCountMsr, 0);
        return;
    }

    auto* registers = reinterpret_cast<volatile ui32*>(apic_base & kApicRegisterMask);
    registers[kApicTimerLvtOffset / sizeof(ui32)] |= kApicLvtMasked;
    registers[kApicInitialCount / sizeof(ui32)] = 0;
}

ui16 read_code_segment() {
    ui16 selector = 0;
    asm volatile("mov %%cs, %0" : "=r"(selector));
    return selector;
}

ui64 default_entry_address() {
    ui64 address = 0;
    asm volatile("lea interrupt_default_entry(%%rip), %0" : "=r"(address));
    return address;
}

ui64 keyboard_entry_address() {
    ui64 address = 0;
    asm volatile("lea interrupt_keyboard_entry(%%rip), %0" : "=r"(address));
    return address;
}

ui64 invalid_opcode_entry_address() {
    ui64 address = 0;
    asm volatile("lea interrupt_invalid_opcode_entry(%%rip), %0" : "=r"(address));
    return address;
}

ui64 general_protection_entry_address() {
    ui64 address = 0;
    asm volatile("lea interrupt_general_protection_entry(%%rip), %0" : "=r"(address));
    return address;
}

ui64 page_fault_entry_address() {
    ui64 address = 0;
    asm volatile("lea interrupt_page_fault_entry(%%rip), %0" : "=r"(address));
    return address;
}

void set_gate(ui8 vector, ui64 address, ui16 selector) {
    IdtEntry& gate     = idt[vector];
    gate.offset_low    = static_cast<ui16>(address);
    gate.selector      = selector;
    gate.ist           = 0;
    gate.attributes    = kPresentInterruptGate;
    gate.offset_middle = static_cast<ui16>(address >> 16);
    gate.offset_high   = static_cast<ui32>(address >> 32);
    gate.reserved      = 0;
}

} // namespace

extern "C" [[noreturn]] void interrupt_exception_handler(ui64 vector, ui64 error_code) {
    serial::write("CPU exception: vector=");
    serial::write(int2str(vector));
    serial::write(" error=");
    serial::write(int2str(error_code));
    if (vector == 14) {
        ui64 fault_address = 0;
        asm volatile("mov %%cr2, %0" : "=r"(fault_address));
        serial::write(" cr2=");
        serial::write(int2str(fault_address));
    }
    serial::write("\n");

    interrupts::disable();
    for (;;) {
        asm volatile("hlt");
    }
}

namespace interrupts {

void disable() {
    asm volatile("cli" ::: "memory");
}

void enable() {
    asm volatile("sti" ::: "memory");
}

void enable_and_halt() {
    asm volatile("sti\n\thlt" ::: "memory");
}

void initialize_idt() {
    const ui16 selector        = read_code_segment();
    const ui64 default_address = default_entry_address();
    for (ui32 vector = 0; vector < 256; ++vector) {
        set_gate(static_cast<ui8>(vector), default_address, selector);
    }
    set_gate(6, invalid_opcode_entry_address(), selector);
    set_gate(13, general_protection_entry_address(), selector);
    set_gate(14, page_fault_entry_address(), selector);
    set_gate(kKeyboardVector, keyboard_entry_address(), selector);

    const Idtr idtr = {
        static_cast<ui16>(sizeof(idt) - 1),
        reinterpret_cast<ui64>(&idt[0]),
    };
    asm volatile("lidt %0" : : "m"(idtr) : "memory");
}

void initialize_pic() {
    stop_firmware_apic_timer();

    io::outb(kPicMasterCommand, kPicInitialize);
    io_wait();
    io::outb(kPicSlaveCommand, kPicInitialize);
    io_wait();

    io::outb(kPicMasterData, kPicMasterOffset);
    io_wait();
    io::outb(kPicSlaveData, kPicSlaveOffset);
    io_wait();

    io::outb(kPicMasterData, kPicMasterHasSlaveAt);
    io_wait();
    io::outb(kPicSlaveData, kPicSlaveIdentity);
    io_wait();

    io::outb(kPicMasterData, kPic8086Mode);
    io_wait();
    io::outb(kPicSlaveData, kPic8086Mode);
    io_wait();

    io::outb(kPicMasterData, 0xff);
    io::outb(kPicSlaveData, 0xff);
}

void enable_keyboard_irq() {
    io::outb(kPicSlaveData, 0xff);
    io::outb(kPicMasterData, static_cast<ui8>(~(1U << 1)));
}

void send_master_eoi() {
    io::outb(kPicMasterCommand, kPicEndOfInterrupt);
}

} // namespace interrupts
