ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUT_DIR ?= $(ROOT_DIR)/out
IMAGE := $(ROOT_DIR)/LambdaY.img
EFI_IMAGE_PATH := ::/EFI/BOOT/BOOTX64.EFI

MAKE ?= make
CC := gcc
CXX := g++
LD := ld
CLANG_FORMAT := clang-format
QEMU := qemu-system-x86_64
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS_4M.fd
OVMF_VARS_COPY := $(OUT_DIR)/OVMF_VARS.fd
IMAGE_SIZE ?= 64M

CPP_FILES := $(shell find "$(ROOT_DIR)/kernel" -type f -name '*.cpp' -print)
CPP_OBJECTS := $(patsubst $(ROOT_DIR)/%.cpp,$(OUT_DIR)/%.o,$(CPP_FILES))
CPP_DEPENDENCIES := $(CPP_OBJECTS:.o=.d)
ASM_FILES := $(shell find "$(ROOT_DIR)/kernel" -type f -name '*.S' -print)
ASM_OBJECTS := $(patsubst $(ROOT_DIR)/%.S,$(OUT_DIR)/%.asm.o,$(ASM_FILES))
KERNEL_OBJECTS := $(CPP_OBJECTS) $(ASM_OBJECTS)
KERNEL_BIN := $(OUT_DIR)/kernel.bin

KERNEL_CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror -ffreestanding \
	-fno-stack-protector -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -fPIE -MMD -MP -mno-red-zone \
	-m64 -I$(ROOT_DIR)/include/global -I$(ROOT_DIR)/include/kernel
KERNEL_LDFLAGS := -mi386pep -nostdlib -T $(ROOT_DIR)/linker.ld \
    --subsystem 10 --image-base 0x100000
KERNEL_ASFLAGS := -m64 -ffreestanding -fPIE -mno-red-zone

.PHONY: all boot kernel image run format clean check-tools

-include $(CPP_DEPENDENCIES)

all: image

boot:
	@echo "Start making the boot sub-target"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" CC="$(CC)" LD="$(LD)"

kernel: $(KERNEL_BIN)

$(OUT_DIR)/%.o: $(ROOT_DIR)/%.cpp
	@mkdir -p "$(dir $@)"
	@echo " CXX $(patsubst $(ROOT_DIR)/%,%,$<) -> $(patsubst $(ROOT_DIR)/%,%,$@)"
	@$(CXX) $(KERNEL_CXXFLAGS) -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(OUT_DIR)/%.asm.o: $(ROOT_DIR)/%.S
	@mkdir -p "$(dir $@)"
	@echo " AS  $(patsubst $(ROOT_DIR)/%,%,$<) -> $(patsubst $(ROOT_DIR)/%,%,$@)"
	@$(CC) $(KERNEL_ASFLAGS) -c "$<" -o "$@"

$(KERNEL_BIN): $(KERNEL_OBJECTS) $(ROOT_DIR)/linker.ld
	@mkdir -p "$(dir $@)"
	@echo " LD  $(patsubst $(ROOT_DIR)/%,%,$@)"
	@$(LD) $(KERNEL_LDFLAGS) $(KERNEL_OBJECTS) -o "$@"

image: boot kernel
	@echo "Start creating a new boot image..."
	@mkdir -p "$(OUT_DIR)/image"
	@rm -f "$(IMAGE)"
	@truncate -s $(IMAGE_SIZE) "$(IMAGE)"
	@mformat -i "$(IMAGE)" -F ::
	@mmd -i "$(IMAGE)" ::/EFI ::/EFI/BOOT
	@mcopy -i "$(IMAGE)" "$(OUT_DIR)/BOOTX64.EFI" $(EFI_IMAGE_PATH)
	@mcopy -i "$(IMAGE)" "$(KERNEL_BIN)" ::/kernel.bin

run: image
	@echo "Launching QEMU (x86_64)..."
	@test -f "$(OVMF_CODE)" || { echo "OVMF code firmware not found: $(OVMF_CODE)" >&2; exit 1; }
	@test -f "$(OVMF_VARS)" || { echo "OVMF variable firmware not found: $(OVMF_VARS)" >&2; exit 1; }
	@mkdir -p "$(OUT_DIR)"
	@cp "$(OVMF_VARS)" "$(OVMF_VARS_COPY)"
	@$(QEMU) -machine q35 -m 512M \
	    -drive if=pflash,format=raw,unit=0,file="$(OVMF_CODE)",readonly=on \
	    -drive if=pflash,format=raw,unit=1,file="$(OVMF_VARS_COPY)" \
	    -drive format=raw,file="$(IMAGE)" \
	    -serial stdio

format:
	@$(CLANG_FORMAT) -i "$(ROOT_DIR)/boot/boot.c" $(CPP_FILES)

check-tools:
	@command -v "$(CC)" >/dev/null || { echo "Required tool not found: $(CC)" >&2; exit 1; }
	@command -v "$(CXX)" >/dev/null || { echo "Required tool not found: $(CXX)" >&2; exit 1; }
	@command -v "$(LD)" >/dev/null || { echo "Required tool not found: $(LD)" >&2; exit 1; }
	@command -v mformat >/dev/null || { echo "Required tool not found: mformat" >&2; exit 1; }
	@command -v mcopy >/dev/null || { echo "Required tool not found: mcopy" >&2; exit 1; }
	@echo "All required build tools are available."

clean:
	@echo "Cleaning old files"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" clean
	@rm -rf "$(OUT_DIR)" "$(IMAGE)"
