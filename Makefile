# DUnix Operating System - Master Makefile
# Target Architecture: x86_64

CC      := gcc
AS      := gcc
LD      := ld
AR      := ar
OBJCOPY := objcopy
HOST_CC := gcc

BUILD_DIR := build

CFLAGS  := -m64 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-pic -fno-pie -mno-red-zone -mcmodel=kernel \
           -mno-mmx -mno-sse -mno-sse2 \
           -Wall -Wextra -Werror -pedantic \
           -Ikernel/include -Ikernel/arch/x86_64 -Ikernel -Ikernel/boot

ASFLAGS := $(CFLAGS) -c

LDFLAGS := -m elf_x86_64 -nostdlib -z max-page-size=0x1000 \
           -z noexecstack --no-warn-rwx-segments \
           -T kernel/arch/x86_64/boot/linker.ld

USER_CFLAGS := -m64 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
               -fno-pic -fno-pie -mno-red-zone \
               -Wall -Wextra -Werror -pedantic \
               -Ilibc/include -I$(BUILD_DIR)/dboot

USER_LDFLAGS := -m elf_x86_64 -nostdlib -z max-page-size=0x1000 \
                -z noexecstack --no-warn-rwx-segments -Ttext=0x400000

KERNEL_ELF := $(BUILD_DIR)/dunix.elf
KERNEL_BIN := $(BUILD_DIR)/dunix.bin
KERNEL_RAW := $(BUILD_DIR)/dunix.raw
DBOOT_MBR_BIN := $(BUILD_DIR)/dboot/mbr.bin
DBOOT_LDR_BIN := $(BUILD_DIR)/dboot/ldr.bin
DBOOT_MBR_H   := $(BUILD_DIR)/dboot/dboot_mbr.h
DBOOT_LDR_H   := $(BUILD_DIR)/dboot/dboot_ldr.h
USB_IMG    := $(BUILD_DIR)/dunix-installer.img
ISO_IMG    := $(BUILD_DIR)/dunix.iso

USER_APPS := init sh cat ls echo mkdir rm pwd cp ps grep head tail wc find touch chmod chown kill sleep env uname dmesg df mount umount true false login vedit ifconfig ping nc wget httpd cc cal bc xargs which basename dirname neofetch dws dterm dfiles dclock dcalc dsession mkfs installer dinstall linux date reboot poweroff clear tee sort uniq tr seq yes more beep du strings hexdump diff cmp uptime sed sudo whoami id dweb browser drm_info glgears snake minesweeper 2048
LINUX_APPS := linux_hello linux_uname links
ALL_APPS   := $(USER_APPS) $(LINUX_APPS)

# Host tool
BIN2C := $(BUILD_DIR)/tools/bin2c

# Libc
LIBC_SRCS_C := $(shell find libc/src -name "*.c" | sort)
LIBC_SRCS_S := $(filter-out libc/src/crt0.S, $(shell find libc/src -name "*.S" | sort))
LIBC_OBJS   := $(patsubst libc/src/%.c, $(BUILD_DIR)/libc/%.o, $(LIBC_SRCS_C)) \
               $(patsubst libc/src/%.S, $(BUILD_DIR)/libc/%.o, $(LIBC_SRCS_S))
LIBC_A      := $(BUILD_DIR)/libc/libc.a
CRT0_O      := $(BUILD_DIR)/libc/crt0.o

# Embedded binaries
USER_BINS     := $(patsubst %, $(BUILD_DIR)/bin/%, $(ALL_APPS))
EMBEDDED_SRCS := $(patsubst %, $(BUILD_DIR)/fs/bin_%.c, $(ALL_APPS))
EMBEDDED_OBJS := $(patsubst %, $(BUILD_DIR)/fs/bin_%.o, $(ALL_APPS))

# Kernel sources & objects
KERNEL_SRCS_C := $(shell find kernel -name "*.c" | sort)
KERNEL_SRCS_S := $(shell find kernel -name "*.S" | sort)
KERNEL_OBJS   := $(patsubst kernel/%.c, $(BUILD_DIR)/kernel/%.o, $(KERNEL_SRCS_C)) \
                 $(patsubst kernel/%.S, $(BUILD_DIR)/kernel/%.o, $(KERNEL_SRCS_S)) \
                 $(EMBEDDED_OBJS)

.PHONY: all clean run run-curses run-serial run-debug test info libc apps iso run-iso run-iso-curses usb run-usb run-usb-curses dboot

all: $(KERNEL_ELF) $(KERNEL_BIN) $(KERNEL_RAW) $(ISO_IMG)

# Host Tools
$(BIN2C): tools/bin2c.c
	@mkdir -p $(dir $@)
	@echo "  HOST_CC $<"
	@$(HOST_CC) -O2 -Wall -Wextra $< -o $@

# dboot MBR and Stage 2 bootloader
$(DBOOT_MBR_BIN): tools/dboot/mbr.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(CC) -m32 -nostdlib -fno-pie -no-pie -Wl,--build-id=none $< -o $(BUILD_DIR)/dboot/mbr.elf -Ttext=0x7C00
	@$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/dboot/mbr.elf $@

$(DBOOT_LDR_BIN): tools/dboot/ldr.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(CC) -m32 -nostdlib -fno-pie -no-pie -Wl,--build-id=none $< -o $(BUILD_DIR)/dboot/ldr.elf -Ttext=0x8000
	@$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/dboot/ldr.elf $@

$(DBOOT_MBR_H): $(DBOOT_MBR_BIN) $(BIN2C)
	@mkdir -p $(dir $@)
	@echo "  BIN2C   $< -> $@"
	@$(BIN2C) dboot_mbr $< $@

$(DBOOT_LDR_H): $(DBOOT_LDR_BIN) $(BIN2C)
	@mkdir -p $(dir $@)
	@echo "  BIN2C   $< -> $@"
	@$(BIN2C) dboot_ldr $< $@

dboot: $(DBOOT_MBR_BIN) $(DBOOT_LDR_BIN) $(DBOOT_MBR_H) $(DBOOT_LDR_H)

$(BUILD_DIR)/user/installer.o: $(DBOOT_MBR_H) $(DBOOT_LDR_H)
$(BUILD_DIR)/user/dinstall.o: $(DBOOT_MBR_H) $(DBOOT_LDR_H)

# Libc compilation
$(CRT0_O): libc/src/crt0.S
	@mkdir -p $(dir $@)
	@echo "  USER_AS $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc/%.o: libc/src/%.c
	@mkdir -p $(dir $@)
	@echo "  USER_CC $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc/%.o: libc/src/%.S
	@mkdir -p $(dir $@)
	@echo "  USER_AS $<"
	@$(AS) $(USER_CFLAGS) -c $< -o $@

$(LIBC_A): $(LIBC_OBJS)
	@mkdir -p $(dir $@)
	@echo "  AR      $@"
	@$(AR) rcs $@ $(LIBC_OBJS)

libc: $(CRT0_O) $(LIBC_A)

# Generation of rules for each user application
define APP_RULE
$$(BUILD_DIR)/user/$(1).o: user/apps/$(1)/$(1).c
	@mkdir -p $$(dir $$@)
	@echo "  USER_CC $$<"
	@$$(CC) $$(USER_CFLAGS) -c $$< -o $$@

$$(BUILD_DIR)/bin/$(1): $$(BUILD_DIR)/user/$(1).o $$(CRT0_O) $$(LIBC_A)
	@mkdir -p $$(dir $$@)
	@echo "  USER_LD $$@"
	@$$(LD) $$(USER_LDFLAGS) -o $$@ $$(CRT0_O) $$< $$(LIBC_A)

$$(BUILD_DIR)/fs/bin_$(1).c: $$(BUILD_DIR)/bin/$(1) $$(BIN2C)
	@mkdir -p $$(dir $$@)
	@echo "  BIN2C   $$< -> $$@"
	@$$(BIN2C) bin_$(1)_data $$< $$@

$$(BUILD_DIR)/fs/bin_$(1).o: $$(BUILD_DIR)/fs/bin_$(1).c
	@mkdir -p $$(dir $$@)
	@echo "  CC      $$<"
	@$$(CC) $$(CFLAGS) -c $$< -o $$@
endef

define LINUX_APP_RULE
ifeq ($$(wildcard user/linux_apps/$(1).c),)
$$(BUILD_DIR)/bin/$(1): user/linux_apps/bin/$(1)
	@mkdir -p $$(dir $$@)
	@echo "  LINUX_BIN $$<"
	@cp $$< $$@
else
$$(BUILD_DIR)/bin/$(1): user/linux_apps/$(1).c
	@mkdir -p $$(dir $$@)
	@echo "  LINUX_CC $$<"
	@$$(CC) -m64 -O2 -nostdlib -static -fno-builtin $$< -o $$@
endif

$$(BUILD_DIR)/fs/bin_$(1).c: $$(BUILD_DIR)/bin/$(1) $$(BIN2C)
	@mkdir -p $$(dir $$@)
	@echo "  BIN2C   $$< -> $$@"
	@$$(BIN2C) bin_$(1)_data $$< $$@

$$(BUILD_DIR)/fs/bin_$(1).o: $$(BUILD_DIR)/fs/bin_$(1).c
	@mkdir -p $$(dir $$@)
	@echo "  CC      $$<"
	@$$(CC) $$(CFLAGS) -c $$< -o $$@
endef

$(foreach app,$(USER_APPS),$(eval $(call APP_RULE,$(app))))
$(foreach app,$(LINUX_APPS),$(eval $(call LINUX_APP_RULE,$(app))))

apps: $(USER_BINS)

# Kernel build
$(KERNEL_ELF): $(KERNEL_OBJS)
	@mkdir -p $(dir $@)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O elf32-i386 $< $@

$(KERNEL_RAW): $(KERNEL_ELF)
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/%.o: kernel/%.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) $< -o $@

clean:
	@rm -rf $(BUILD_DIR) serial.log
	@echo "Cleaned build directory."

run: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -serial mon:stdio -nic model=e1000

# Bootable ISO image (El Torito / isohybrid)
iso: $(KERNEL_BIN) tools/make_iso.sh tools/isolinux.cfg
	@./tools/make_iso.sh $(ISO_IMG)

$(ISO_IMG): $(KERNEL_BIN) tools/make_iso.sh tools/isolinux.cfg
	@./tools/make_iso.sh $(ISO_IMG)

run-iso: $(ISO_IMG)
	qemu-system-x86_64 -cdrom $(ISO_IMG) -serial mon:stdio -nic model=e1000

run-iso-curses: $(ISO_IMG)
	qemu-system-x86_64 -cdrom $(ISO_IMG) -display curses -nic model=e1000

# Bootable USB installer image
usb: $(KERNEL_BIN) tools/make_usb_image.sh tools/syslinux.cfg
	@./tools/make_usb_image.sh $(USB_IMG)

image: usb

$(USB_IMG): $(KERNEL_BIN) tools/make_usb_image.sh tools/syslinux.cfg
	@./tools/make_usb_image.sh $(USB_IMG)

run-usb: $(USB_IMG)
	qemu-system-x86_64 -drive file=$(USB_IMG),format=raw -serial mon:stdio -nic model=e1000

run-usb-curses: $(USB_IMG)
	qemu-system-x86_64 -drive file=$(USB_IMG),format=raw -display curses -nic model=e1000

run-curses: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -display curses -nic model=e1000

run-serial: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -nographic -nic model=e1000

run-debug: $(KERNEL_BIN)
	qemu-system-x86_64 -kernel $(KERNEL_BIN) -s -S -serial stdio -nic model=e1000 &

test: $(KERNEL_BIN)
	@echo "Running DUnix automated test suite (Milestones 1-21)..."
	@rm -f serial.log
	@timeout 5s qemu-system-x86_64 -kernel $(KERNEL_BIN) -serial file:serial.log -display none || true
	@if grep -q "Milestones 1 through 21 Complete" serial.log; then \
		echo "=== DUnix Test Suite PASSED ==="; \
		cat serial.log; \
	else \
		echo "=== DUnix Test Suite FAILED ==="; \
		cat serial.log; \
		exit 1; \
	fi

info: $(KERNEL_ELF)
	@echo "=== DUnix Kernel ELF Summary ==="
	@readelf -h $(KERNEL_ELF)
	@echo "=== Program Headers ==="
	@readelf -l $(KERNEL_ELF)
	@echo "=== Section Headers ==="
	@readelf -S $(KERNEL_ELF)
