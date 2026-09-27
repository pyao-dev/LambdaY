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

// 封装键盘状态，提升可维护性和清晰度
struct KeyboardState {
    // 扫描码队列
    volatile ui8 scancode_queue[kQueueSize] = {};
    volatile ui8 queue_head                 = 0;
    volatile ui8 queue_tail                 = 0;

    // ASCII 字符队列
    volatile ui8 ascii_queue[kQueueSize] = {};
    volatile ui8 ascii_head              = 0;
    volatile ui8 ascii_tail              = 0;

    // 扫描码解析状态
    bool extended_scancode = false;
    bool break_code        = false;
    bool pause_sequence    = false;
    ui8  pause_bytes       = 0;

    // 修饰键状态
    bool left_shift    = false;
    bool right_shift   = false;
    bool left_control  = false;
    bool right_control = false;
    bool left_alt      = false;
    bool right_alt     = false;
    bool caps_lock     = false;

    void reset() {
        queue_head        = 0;
        queue_tail        = 0;
        ascii_head        = 0;
        ascii_tail        = 0;
        extended_scancode = false;
        break_code        = false;
        pause_sequence    = false;
        pause_bytes       = 0;
        left_shift        = false;
        right_shift       = false;
        left_control      = false;
        right_control     = false;
        left_alt          = false;
        right_alt         = false;
        caps_lock         = false;
    }
};

KeyboardState g_keyboard_state;

struct AsciiMapping {
    char normal;
    char shifted;
    bool valid;
};

// 优化：使用直接数组索引替代线性搜索
// 索引为 PS/2 扫描码集2 的扫描码值
AsciiMapping kScancodeToAscii[256] = {};

void initialize_scancode_map() {
    kScancodeToAscii[0x0d] = {'\t', '\t', true};
    kScancodeToAscii[0x0e] = {'`', '~', true};
    kScancodeToAscii[0x15] = {'q', 'Q', true};
    kScancodeToAscii[0x16] = {'1', '!', true};
    kScancodeToAscii[0x1a] = {'z', 'Z', true};
    kScancodeToAscii[0x1b] = {'s', 'S', true};
    kScancodeToAscii[0x1c] = {'a', 'A', true};
    kScancodeToAscii[0x1d] = {'w', 'W', true};
    kScancodeToAscii[0x1e] = {'2', '@', true};
    kScancodeToAscii[0x21] = {'c', 'C', true};
    kScancodeToAscii[0x22] = {'x', 'X', true};
    kScancodeToAscii[0x23] = {'d', 'D', true};
    kScancodeToAscii[0x24] = {'e', 'E', true};
    kScancodeToAscii[0x25] = {'4', '$', true};
    kScancodeToAscii[0x26] = {'3', '#', true};
    kScancodeToAscii[0x29] = {' ', ' ', true};
    kScancodeToAscii[0x2a] = {'v', 'V', true};
    kScancodeToAscii[0x2b] = {'f', 'F', true};
    kScancodeToAscii[0x2c] = {'t', 'T', true};
    kScancodeToAscii[0x2d] = {'r', 'R', true};
    kScancodeToAscii[0x2e] = {'5', '%', true};
    kScancodeToAscii[0x31] = {'n', 'N', true};
    kScancodeToAscii[0x32] = {'b', 'B', true};
    kScancodeToAscii[0x33] = {'h', 'H', true};
    kScancodeToAscii[0x34] = {'g', 'G', true};
    kScancodeToAscii[0x35] = {'y', 'Y', true};
    kScancodeToAscii[0x36] = {'6', '^', true};
    kScancodeToAscii[0x3a] = {'m', 'M', true};
    kScancodeToAscii[0x3b] = {'j', 'J', true};
    kScancodeToAscii[0x3c] = {'u', 'U', true};
    kScancodeToAscii[0x3d] = {'7', '&', true};
    kScancodeToAscii[0x3e] = {'8', '*', true};
    kScancodeToAscii[0x41] = {',', '<', true};
    kScancodeToAscii[0x42] = {'k', 'K', true};
    kScancodeToAscii[0x43] = {'i', 'I', true};
    kScancodeToAscii[0x44] = {'o', 'O', true};
    kScancodeToAscii[0x45] = {'0', ')', true};
    kScancodeToAscii[0x46] = {'9', '(', true};
    kScancodeToAscii[0x49] = {'.', '>', true};
    kScancodeToAscii[0x4a] = {'/', '?', true};
    kScancodeToAscii[0x4b] = {'l', 'L', true};
    kScancodeToAscii[0x4c] = {';', ':', true};
    kScancodeToAscii[0x4d] = {'p', 'P', true};
    kScancodeToAscii[0x4e] = {'-', '_', true};
    kScancodeToAscii[0x52] = {'\'', '"', true};
    kScancodeToAscii[0x54] = {'[', '{', true};
    kScancodeToAscii[0x55] = {'=', '+', true};
    kScancodeToAscii[0x5a] = {'\n', '\n', true};
    kScancodeToAscii[0x5b] = {']', '}', true};
    kScancodeToAscii[0x5d] = {'\\', '|', true};
    kScancodeToAscii[0x66] = {'\b', '\b', true};
    kScancodeToAscii[0x76] = {'\x1b', '\x1b', true};
}

bool is_letter(char character) {
    return character >= 'a' && character <= 'z';
}

bool is_control_active() {
    return g_keyboard_state.left_control || g_keyboard_state.right_control;
}

char control_character(char character) {
    if (is_letter(character)) {
        return static_cast<char>(character - 'a' + 1);
    }
    switch (character) {
    case '@':
        return '\0';
    case '[':
        return '\x1b';
    case '\\':
        return '\x1c';
    case ']':
        return '\x1d';
    case '^':
        return '\x1e';
    case '_':
        return '\x1f';
    default:
        return character;
    }
}

void enqueue(volatile ui8* queue, volatile ui8& head, volatile ui8& tail, ui8 value) {
    const ui8 current = head;
    const ui8 next    = static_cast<ui8>(current + 1);
    if (next != tail) {
        queue[current] = value;
        head           = next;
    }
}

char ascii_for_scancode(ui8 scancode) {
    const AsciiMapping& mapping = kScancodeToAscii[scancode];
    if (!mapping.valid) {
        return '\0';
    }

    const bool shift = g_keyboard_state.left_shift || g_keyboard_state.right_shift;
    if (is_letter(mapping.normal)) {
        const bool uppercase = shift != g_keyboard_state.caps_lock;
        return uppercase ? mapping.shifted : mapping.normal;
    }
    return shift ? mapping.shifted : mapping.normal;
}

void handle_scancode(ui8 scancode) {
    if (g_keyboard_state.pause_sequence) {
        if (g_keyboard_state.pause_bytes != 0) {
            --g_keyboard_state.pause_bytes;
        }
        if (g_keyboard_state.pause_bytes == 0) {
            g_keyboard_state.pause_sequence = false;
        }
        return;
    }
    if (scancode == 0xe1) {
        g_keyboard_state.pause_sequence = true;
        g_keyboard_state.pause_bytes    = 7;
        return;
    }
    if (scancode == 0xe0) {
        g_keyboard_state.extended_scancode = true;
        return;
    }
    if (scancode == 0xf0) {
        g_keyboard_state.break_code = true;
        return;
    }

    const bool extended                = g_keyboard_state.extended_scancode;
    const bool released                = g_keyboard_state.break_code;
    g_keyboard_state.extended_scancode = false;
    g_keyboard_state.break_code        = false;

    if (extended) {
        if (scancode == 0x14) {
            g_keyboard_state.right_control = !released;
        } else if (scancode == 0x11) {
            g_keyboard_state.right_alt = !released;
        }
        return;
    }

    switch (scancode) {
    case 0x12:
        g_keyboard_state.left_shift = !released;
        return;
    case 0x59:
        g_keyboard_state.right_shift = !released;
        return;
    case 0x14:
        g_keyboard_state.left_control = !released;
        return;
    case 0x11:
        g_keyboard_state.left_alt = !released;
        return;
    case 0x58:
        if (!released) {
            g_keyboard_state.caps_lock = !g_keyboard_state.caps_lock;
        }
        return;
    default:
        break;
    }
    if (released || g_keyboard_state.left_alt || g_keyboard_state.right_alt) {
        return;
    }

    char character = ascii_for_scancode(scancode);
    if (character == '\0') {
        return;
    }
    if (is_control_active()) {
        character = control_character(character);
    }
    enqueue(g_keyboard_state.ascii_queue, g_keyboard_state.ascii_head, g_keyboard_state.ascii_tail,
            static_cast<ui8>(character));
}

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
    initialize_scancode_map();
    g_keyboard_state.reset();

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
    const ui8 tail = g_keyboard_state.queue_tail;
    if (tail == g_keyboard_state.queue_head) {
        return false;
    }

    scancode                    = g_keyboard_state.scancode_queue[tail];
    g_keyboard_state.queue_tail = static_cast<ui8>(tail + 1);
    return true;
}

bool has_scancode() {
    return g_keyboard_state.queue_head != g_keyboard_state.queue_tail;
}

bool pop_ascii(ui8& character) {
    const ui8 tail = g_keyboard_state.ascii_tail;
    if (tail == g_keyboard_state.ascii_head) {
        return false;
    }

    character                   = g_keyboard_state.ascii_queue[tail];
    g_keyboard_state.ascii_tail = static_cast<ui8>(tail + 1);
    return true;
}

bool has_ascii() {
    return g_keyboard_state.ascii_head != g_keyboard_state.ascii_tail;
}

} // namespace keyboard

extern "C" void keyboard_handle_irq() {
    const ui8 scancode = io::inb(kDataPort);
    const ui8 head     = g_keyboard_state.queue_head;
    const ui8 next     = static_cast<ui8>(head + 1);
    if (next != g_keyboard_state.queue_tail) {
        g_keyboard_state.scancode_queue[head] = scancode;
        g_keyboard_state.queue_head           = next;
    }
    handle_scancode(scancode);
    interrupts::send_master_eoi();
}
