#include <devices/interrupts.h>
#include <devices/io.h>
#include <devices/keyboard.h>
#include <types.h>

namespace {

constexpr ui16 kDataPort    = 0x60;
constexpr ui16 kStatusPort  = 0x64;
constexpr ui16 kCommandPort = 0x64;

constexpr ui8 kOutputBufferFull = 1U << 0;
constexpr ui8 kInputBufferFull  = 1U << 1;
constexpr ui8 kAuxiliaryData    = 1U << 5;

constexpr ui8 kDisableSecondPort  = 0xa7;
constexpr ui8 kDisableFirstPort   = 0xad;
constexpr ui8 kEnableFirstPort    = 0xae;
constexpr ui8 kReadConfiguration  = 0x20;
constexpr ui8 kWriteConfiguration = 0x60;

constexpr ui8 kFirstPortInterrupt = 1U << 0;
constexpr ui8 kFirstPortDisabled  = 1U << 4;
constexpr ui8 kSecondPortDisabled = 1U << 5;
constexpr ui8 kTranslationEnabled = 1U << 6;

constexpr ui8 kDisableScanning = 0xf5;
constexpr ui8 kSetScancodeSet  = 0xf0;
constexpr ui8 kScancodeSetTwo  = 0x02;
constexpr ui8 kEnableScanning  = 0xf4;
constexpr ui8 kAcknowledged    = 0xfa;
constexpr ui8 kResend          = 0xfe;

constexpr ui32 kIoTimeout       = 100000;
constexpr ui32 kCommandAttempts = 3;
constexpr ui32 kQueueSize       = 256;

volatile ui8 scancode_queue[kQueueSize] = {};
volatile ui8 queue_head                 = 0;
volatile ui8 queue_tail                 = 0;

bool wait_for_input_buffer() {
    for (ui32 attempt = 0; attempt < kIoTimeout; ++attempt) {
        if ((io::inb(kStatusPort) & kInputBufferFull) == 0) {
            return true;
        }
        asm volatile("pause");
    }
    return false;
}

bool read_output(ui8& value) {
    for (ui32 attempt = 0; attempt < kIoTimeout; ++attempt) {
        const ui8 status = io::inb(kStatusPort);
        if ((status & kOutputBufferFull) != 0) {
            value = io::inb(kDataPort);
            if ((status & kAuxiliaryData) == 0) {
                return true;
            }
        }
        asm volatile("pause");
    }
    return false;
}

bool write_command(ui8 command) {
    if (!wait_for_input_buffer()) {
        return false;
    }
    io::outb(kCommandPort, command);
    return true;
}

bool write_data(ui8 data) {
    if (!wait_for_input_buffer()) {
        return false;
    }
    io::outb(kDataPort, data);
    return true;
}

void flush_output_buffer() {
    for (ui32 attempt = 0; attempt < kIoTimeout; ++attempt) {
        if ((io::inb(kStatusPort) & kOutputBufferFull) == 0) {
            return;
        }
        static_cast<void>(io::inb(kDataPort));
    }
}

bool send_device_byte(ui8 value) {
    for (ui32 attempt = 0; attempt < kCommandAttempts; ++attempt) {
        if (!write_data(value)) {
            return false;
        }

        ui8 response = 0;
        if (!read_output(response)) {
            return false;
        }
        if (response == kAcknowledged) {
            return true;
        }
        if (response != kResend) {
            return false;
        }
    }
    return false;
}

bool read_configuration(ui8& configuration) {
    return write_command(kReadConfiguration) && read_output(configuration);
}

bool write_configuration(ui8 configuration) {
    return write_command(kWriteConfiguration) && write_data(configuration);
}

} // namespace

namespace keyboard {

bool initialize() {
    queue_head = 0;
    queue_tail = 0;

    if (!write_command(kDisableFirstPort) || !write_command(kDisableSecondPort)) {
        return false;
    }
    flush_output_buffer();

    ui8 configuration = 0;
    if (!read_configuration(configuration)) {
        return false;
    }
    configuration &= static_cast<ui8>(~(kFirstPortInterrupt | kTranslationEnabled));
    configuration |= kSecondPortDisabled;
    if (!write_configuration(configuration) || !write_command(kEnableFirstPort)) {
        return false;
    }

    if (!send_device_byte(kDisableScanning) || !send_device_byte(kSetScancodeSet) ||
        !send_device_byte(kScancodeSetTwo) || !send_device_byte(kEnableScanning)) {
        return false;
    }

    configuration |= kFirstPortInterrupt;
    configuration &= static_cast<ui8>(~kFirstPortDisabled);
    return write_configuration(configuration);
}

bool pop_scancode(ui8& scancode) {
    const ui8 tail = queue_tail;
    if (tail == queue_head) {
        return false;
    }

    scancode   = scancode_queue[tail];
    queue_tail = static_cast<ui8>(tail + 1);
    return true;
}

bool has_scancode() {
    return queue_head != queue_tail;
}

} // namespace keyboard

extern "C" void keyboard_handle_irq() {
    const ui8 scancode = io::inb(kDataPort);
    const ui8 head     = queue_head;
    const ui8 next     = static_cast<ui8>(head + 1);
    if (next != queue_tail) {
        scancode_queue[head] = scancode;
        queue_head           = next;
    }
    interrupts::send_master_eoi();
}
