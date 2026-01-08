CC := clang
LD := ld.lld

TARGET := x86_64-elf

CFLAGS := -target $(TARGET) -std=c11 -O2 -g \
          -ffreestanding -fno-stack-protector -fno-pic \
          -mno-red-zone -mcmodel=kernel \
          -Wall -Wextra -Werror \
          -Iinclude

ASFLAGS := -target $(TARGET) -ffreestanding

LDFLAGS := -T linker.ld -nostdlib

BUILD := build
OUT := $(BUILD)/kernel.elf
OBJ := $(BUILD)/entry.o $(BUILD)/kmain.o $(BUILD)/pmm.o $(BUILD)/kmem.o \
       $(BUILD)/uart.o $(BUILD)/shell.o $(BUILD)/str.o

all: $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

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