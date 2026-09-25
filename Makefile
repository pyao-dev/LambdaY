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

CPP_FILES := $(shell find "$(ROOT_DIR)" -path "$(ROOT_DIR)/gnu-efi" -prune -o -type f -name '*.cpp' -print)
H_FILES := $(shell find "$(ROOT_DIR)" -path "$(ROOT_DIR)/gnu-efi" -prune -o -type f -name '*.h' -print)
CPP_OBJECTS := $(patsubst $(ROOT_DIR)/%.cpp,$(OUT_DIR)/%.o,$(CPP_FILES))
KERNEL_BIN := $(OUT_DIR)/kernel.bin

KERNEL_CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Werror -ffreestanding \
    -fno-stack-protector -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
    -fno-asynchronous-unwind-tables -fno-unwind-tables -fPIE -mno-red-zone \
	-m64 -DGNU_EFI_USE_MS_ABI -I$(ROOT_DIR)/include/global \
	-I$(ROOT_DIR)/include/kernel -I$(ROOT_DIR)/gnu-efi/inc \
	-I$(ROOT_DIR)/gnu-efi/inc/x86_64 -I$(ROOT_DIR)/gnu-efi/inc/protocol
KERNEL_LDFLAGS := -mi386pep -nostdlib -T $(ROOT_DIR)/linker.ld \
    --subsystem 10 --image-base 0x100000

.PHONY: all boot kernel image run format clean check-tools

all: image

boot:
	@echo "Start making the boot sub-target"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" CC="$(CC)" LD="$(LD)"

kernel: $(KERNEL_BIN)

$(OUT_DIR)/%.o: $(ROOT_DIR)/%.cpp $(H_FILES)
	@mkdir -p "$(dir $@)"
	@echo " CXX $< -> $@"
	@$(CXX) $(KERNEL_CXXFLAGS) -c "$<" -o "$@"

$(KERNEL_BIN): $(CPP_OBJECTS) $(ROOT_DIR)/linker.ld
	@mkdir -p "$(dir $@)"
	@echo " LD $@"
	@$(LD) $(KERNEL_LDFLAGS) $(CPP_OBJECTS) -o "$@"

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
	@$(CLANG_FORMAT) -i "$(ROOT_DIR)/boot/boot.c" $(CPP_FILES) $(H_FILES)

clean:
	@echo "Cleaning old files"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" clean
	@rm -rf "$(OUT_DIR)" "$(IMAGE)"
