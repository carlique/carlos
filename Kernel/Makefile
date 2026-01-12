# ---- Toolchain ----
CC      := clang
LD      := ld.lld
TARGET  := x86_64-elf

# ---- Paths ----
BUILD   := build
OUT     := $(BUILD)/kernel.elf
LDSCRIPT:= linker.ld
ESP_KERNEL := ../qemu/esp/EFI/CARLOS/KERNEL.ELF

# ---- Flags ----
LOG_LEVEL ?= KLOG_INFO
LOG_MASK  ?= KLOG_MOD_ALL

INCLUDES := -Iinclude -I../Common/include -I../Libc/include -Ithird_party/font8x8

CFLAGS  := -target $(TARGET) -std=c11 -O2 -g \
           -ffreestanding -fno-stack-protector -fno-pic \
           -mno-red-zone -mcmodel=kernel \
           -Wall -Wextra -Werror \
           $(INCLUDES)

CFLAGS += -DKLOG_DEFAULT_LEVEL=$(LOG_LEVEL) -DKLOG_DEFAULT_MASK=$(LOG_MASK)

ASFLAGS := -target $(TARGET) -ffreestanding
LDFLAGS := -T $(LDSCRIPT) -nostdlib

# ---- Sources ----
SRCS_C := \
  src/kmain.c src/pmm.c src/kmem.c src/uart.c src/shell.c src/str.c src/klog.c \
  src/kbd.c src/fbcon.c src/kapi.c src/acpi.c src/idt.c src/isr.c src/gdt.c \
  src/hpet.c src/time.c src/pci.c src/ahci.c \
  src/fs.c src/fat16.c src/part.c src/disk.c src/disk_ahci.c src/path.c \
  src/mem.c src/ls.c src/part_gpt.c src/mkdir.c src/exec.c src/exec_elf.c \
  src/intr.c src/pic.c src/irq.c src/pit.c

# Only entry.S goes through the generic %.S -> %.o rule
SRCS_S := src/entry.S src/exec_s.S

OBJS_C := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS_C))
OBJS_S := $(patsubst src/%.S,$(BUILD)/%.o,$(SRCS_S))

# and special-case it like isr.S if you also have irq.c:
OBJS   := $(OBJS_C) $(OBJS_S) $(BUILD)/isr_s.o $(BUILD)/irq_s.o

# ---- Generic compile rules ----
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

# Special-case rule
$(BUILD)/isr_s.o: src/isr.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/irq_s.o: src/irq.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/fs.o: CFLAGS += -DCARLOS_FS_INTERNAL=1

# ---- Default target ----
.PHONY: all
all: $(OUT)

# ---- Build dir ----
$(BUILD):
	mkdir -p $(BUILD)

# ---- Link ----
$(OUT): $(OBJS) $(LDSCRIPT)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# ---- Install ----
.PHONY: install
install: $(OUT)
	mkdir -p $(dir $(ESP_KERNEL))
	cp $(OUT) $(ESP_KERNEL)

# ---- Clean ----
.PHONY: clean
clean:
	rm -rf $(BUILD)