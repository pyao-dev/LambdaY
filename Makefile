ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUT_DIR ?= $(ROOT_DIR)/out
IMAGE := $(ROOT_DIR)/LambdaY.img
EFI_IMAGE_PATH := ::/EFI/BOOT/BOOTX64.EFI

MAKE ?= make
CC := gcc
LD := ld
CLANG_FORMAT := clang-format
QEMU := qemu-system-x86_64
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS_4M.fd
OVMF_VARS_COPY := $(OUT_DIR)/OVMF_VARS.fd
IMAGE_SIZE ?= 64M

.PHONY: all boot image run format clean check-tools

all: image

boot:
	@echo "Start making the boot sub-target"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" CC="$(CC)" LD="$(LD)"

image: boot
	@echo "Start creating a new boot image..."
	@mkdir -p "$(OUT_DIR)/image"
	@rm -f "$(IMAGE)"
	@truncate -s $(IMAGE_SIZE) "$(IMAGE)"
	@mformat -i "$(IMAGE)" -F ::
	@mmd -i "$(IMAGE)" ::/EFI ::/EFI/BOOT
	@mcopy -i "$(IMAGE)" "$(OUT_DIR)/BOOTX64.EFI" $(EFI_IMAGE_PATH)

run: image
	@echo "Launching QEMU (x86_64)..."
	@test -f "$(OVMF_CODE)" || { echo "OVMF code firmware not found: $(OVMF_CODE)" >&2; exit 1; }
	@test -f "$(OVMF_VARS)" || { echo "OVMF variable firmware not found: $(OVMF_VARS)" >&2; exit 1; }
	@mkdir -p "$(OUT_DIR)"
	@cp "$(OVMF_VARS)" "$(OVMF_VARS_COPY)"
	@$(QEMU) -machine q35 -m 512M \
	    -drive if=pflash,format=raw,unit=0,file="$(OVMF_CODE)",readonly=on \
	    -drive if=pflash,format=raw,unit=1,file="$(OVMF_VARS_COPY)" \
	    -drive format=raw,file="$(IMAGE)"

format:
	@$(CLANG_FORMAT) -i "$(ROOT_DIR)/boot/boot.c"

clean:
	@echo "Cleaning old files"
	@$(MAKE) -C "$(ROOT_DIR)/boot" OUT_DIR="$(OUT_DIR)" clean
	@rm -rf "$(OUT_DIR)" "$(IMAGE)"
