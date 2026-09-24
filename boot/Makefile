ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OUT_DIR ?= $(ROOT_DIR)/out
BOOT_OUT_DIR := $(OUT_DIR)/boot
EFI_OUT := $(OUT_DIR)/BOOTX64.EFI
ELF_OUT := $(BOOT_OUT_DIR)/boot.so
OBJECT_OUT := $(BOOT_OUT_DIR)/boot.o

CC := gcc
LD := ld
OBJCOPY := objcopy
GNU_EFI_DIR := $(ROOT_DIR)/gnu-efi
GNU_EFI_BUILD_DIR := $(OUT_DIR)/gnu-efi
GNU_EFI_ARCH_DIR := $(GNU_EFI_BUILD_DIR)/x86_64
GNU_EFI_LIB_DIR := $(GNU_EFI_ARCH_DIR)/lib
GNU_EFI_CRT_DIR := $(GNU_EFI_ARCH_DIR)/gnuefi
GNU_EFI_LIBS := $(GNU_EFI_LIB_DIR)/libefi.a $(GNU_EFI_CRT_DIR)/libgnuefi.a
GNU_EFI_CRT := $(GNU_EFI_CRT_DIR)/crt0-efi-x86_64.o
GNU_EFI_LINKER_SCRIPT := $(GNU_EFI_DIR)/gnuefi/elf_x86_64_efi.lds
GNU_EFI_STAMP := $(GNU_EFI_BUILD_DIR)/.built

EFI_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
    -fno-stack-protector -fno-builtin -fshort-wchar -mno-red-zone \
    -fno-asynchronous-unwind-tables -fno-unwind-tables \
    -DGNU_EFI_USE_MS_ABI -I$(GNU_EFI_DIR)/inc -I$(GNU_EFI_DIR)/inc/x86_64 \
    -I$(GNU_EFI_DIR)/inc/protocol
EFI_LDFLAGS := -nostdlib -z nocombreloc -shared -Bsymbolic \
    -T $(GNU_EFI_LINKER_SCRIPT) -L$(GNU_EFI_LIB_DIR) -L$(GNU_EFI_CRT_DIR)
EFI_LIBS := -lefi -lgnuefi $(shell $(CC) -print-libgcc-file-name)

.PHONY: all gnu-efi clean

all: $(EFI_OUT)

$(GNU_EFI_STAMP):
	@echo "Starting to call the gnu-efi submodule build..."
	@mkdir -p "$(GNU_EFI_BUILD_DIR)"
	@$(MAKE) -C "$(GNU_EFI_BUILD_DIR)" -f "$(GNU_EFI_DIR)/Makefile" \
	    VPATH="$(GNU_EFI_DIR)" TOPDIR="$(GNU_EFI_DIR)" \
	    OBJDIR="$(GNU_EFI_ARCH_DIR)" ARCH=x86_64 \
	    CC="$(CC)" LD="$(LD)" AR=ar RANLIB=ranlib \
	    OBJCOPY="$(OBJCOPY)" lib gnuefi >/dev/null
	@touch "$@"

$(GNU_EFI_LIBS) $(GNU_EFI_CRT): $(GNU_EFI_STAMP)

gnu-efi: $(GNU_EFI_STAMP)

$(BOOT_OUT_DIR):
	@mkdir -p "$@"

$(OBJECT_OUT): $(ROOT_DIR)/boot/boot.c | $(BOOT_OUT_DIR)
	@echo " CC $< -> $@"
	@$(CC) $(EFI_CFLAGS) -c "$<" -o "$@"

$(ELF_OUT): $(OBJECT_OUT) $(GNU_EFI_LIBS) $(GNU_EFI_CRT) $(GNU_EFI_LINKER_SCRIPT)
	@echo " LD $< -> $@"
	@$(LD) $(EFI_LDFLAGS) $(GNU_EFI_CRT) $(OBJECT_OUT) -o "$@" $(EFI_LIBS)

$(EFI_OUT): $(ELF_OUT)
	@echo "Making BOOTX64.EFI"
	@$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .rodata \
	    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
	    -O efi-app-x86_64 "$<" "$@"

clean:
	@rm -rf "$(BOOT_OUT_DIR)" "$(EFI_OUT)"
