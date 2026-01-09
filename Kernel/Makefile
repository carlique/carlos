CC := clang
LD := ld.lld

TARGET := x86_64-elf

CFLAGS := -target $(TARGET) -std=c11 -O2 -g \
          -ffreestanding -fno-stack-protector -fno-pic \
          -mno-red-zone -mcmodel=kernel \
          -Wall -Wextra -Werror \
          -Iinclude

CFLAGS += -I../Common/include

CFLAGS += -Ithird_party/font8x8

ASFLAGS := -target $(TARGET) -ffreestanding

LDFLAGS := -T linker.ld -nostdlib

BUILD := build
OUT := $(BUILD)/kernel.elf
OBJ := $(BUILD)/entry.o $(BUILD)/kmain.o $(BUILD)/pmm.o $(BUILD)/kmem.o \
       $(BUILD)/uart.o $(BUILD)/shell.o $(BUILD)/str.o $(BUILD)/klog.o \
	   $(BUILD)/kbd.o $(BUILD)/fbcon.o $(BUILD)/kapi.o $(BUILD)/acpi.o \
	   $(BUILD)/idt.o $(BUILD)/isr.o $(BUILD)/isr_s.o $(BUILD)/gdt.o

all: $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/gdt.o: src/gdt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
	
$(BUILD)/idt.o: src/idt.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/isr.o: src/isr.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/isr_s.o: src/isr.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/acpi.o: src/acpi.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kapi.o: src/kapi.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
	
$(BUILD)/fbcon.o: src/fbcon.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kbd.o: src/kbd.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/klog.o: src/klog.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: src/uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: src/shell.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/str.o: src/str.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/pmm.o: src/pmm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
	
$(BUILD)/kmem.o: src/kmem.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/entry.o: src/entry.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/kmain.o: src/kmain.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ) linker.ld
	$(LD) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean