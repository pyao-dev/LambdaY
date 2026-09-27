# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LambdaY is a 64-bit x86_64 operating system that boots via UEFI. It consists of two main components:
1. **UEFI Bootloader** (`boot/boot.c`) - Loads and starts the kernel using gnu-efi
2. **Kernel** (`kernel/`) - Freestanding C++17 kernel with graphics, interrupts, keyboard input, and basic UI

The kernel runs in long mode (64-bit) at base address 0x101000 and communicates via serial output (COM1) for debugging.

## Build Commands

**Build the OS image:**
```bash
make
```
This builds the bootloader, kernel, and creates `LambdaY.img` (64MB FAT filesystem).

**Build only bootloader:**
```bash
make boot
```

**Build only kernel:**
```bash
make kernel
```

**Run in QEMU:**
```bash
make run
```
Requires OVMF firmware at `/usr/share/OVMF/OVMF_CODE_4M.fd` and `OVMF_VARS_4M.fd`.

**Format code:**
```bash
make format
```
Runs clang-format on all C/C++ source files.

**Clean build artifacts:**
```bash
make clean
```

**Check required tools:**
```bash
make check-tools
```

## Architecture

### Boot Flow
1. UEFI firmware loads `BOOTX64.EFI` from the EFI system partition
2. Bootloader (`boot/boot.c`) reads `kernel.bin` from the root of the FAT filesystem
3. Bootloader uses `LoadImage` and `StartImage` to transfer control to `kernel_entry`
4. Kernel initializes in this order: serial → graphics → interrupts → keyboard → terminal

### Module Structure

**`kernel/`**
- `main.cpp` - Entry point (`kernel_entry`), main loop for keyboard input processing
- `devices/` - Hardware drivers (interrupts, keyboard, serial)
  - `interrupts.cpp/.S` - IDT setup, PIC initialization, IRQ handling
  - `keyboard.cpp` - PS/2 keyboard driver with scan code to ASCII translation
  - `serial.cpp` - COM1 serial port for debugging
- `graphics/` - Framebuffer graphics
  - `basic.cpp` - GOP (Graphics Output Protocol) initialization, pixel operations
  - `pf/lib.cpp` - Font rendering using ASC16 (ASCII) and HZK16 (Chinese characters)
- `ui/` - User interface components
  - `terminal.cpp` - Basic terminal for text input/output

**`include/`**
- `global/` - Shared utilities (types, string conversion)
- `kernel/` - Kernel headers matching the source structure

**`boot/`**
- `boot.c` - UEFI bootloader using gnu-efi

### Key Technical Details

**Kernel compilation flags:**
- C++17 with `-ffreestanding`, no stdlib, no exceptions, no RTTI
- Position-independent executable (`-fPIE`)
- Red zone disabled (`-mno-red-zone`) for interrupt safety
- Custom linker script at `linker.ld`

**Memory model:**
- Kernel loads at physical address 0x100000 (1MB)
- Entry point at 0x101000
- Uses GOP framebuffer for graphics (linear framebuffer direct access)

**Interrupt handling:**
- IDT configured for 256 entries
- PIC (8259) initialized and remapped (master: IRQ 0x20-0x27, slave: 0x28-0x2f)
- Keyboard on IRQ1, handled via interrupt-driven input queue

**Font rendering:**
- ASC16: 8x16 ASCII font
- HZK16: 16x16 Chinese font (GB2312 encoding)
- Both font data files are in `include/kernel/graphics/pf/fontdata/` - **DO NOT read these files in full** (they are large binary data arrays). Only read the first 20 lines if needed.

## Development Notes

- Serial output (COM1) is the primary debugging interface - messages appear in QEMU's `-serial stdio`
- The kernel runs with interrupts disabled initially, then enables them after keyboard setup
- The main loop polls the keyboard ASCII queue and renders characters to the terminal
- All kernel code must be freestanding (no standard library)
- Chinese text rendering is supported via the HZK16 font

## Dependencies

Required packages (Ubuntu/Debian):
```bash
sudo apt install build-essential clang-format qemu-system-x86 mtools
```

- `gcc`/`g++` - Compiler and linker
- `clang-format` - Code formatting
- `qemu-system-x86_64` - Emulation
- `mtools` - FAT filesystem tools (mformat, mcopy, mmd)
- OVMF firmware for UEFI emulation

## Important Constraints

- The `gnu-efi` directory is a git submodule - clone with `--recursive` flag
- Font data files (`include/kernel/graphics/pf/fontdata/*.h`) are very large - avoid reading them in full
- The bootloader limits kernel size to 16MB (`KERNEL_MAX_SIZE`)
- QEMU configuration uses Q35 machine type with 512MB RAM
