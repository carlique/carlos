CC      := clang
LD      := ld.lld
TARGET  := x86_64-elf

# Paths (relative to Apps/bin/ls)
ROOT    := ../../..
COMMONI := $(ROOT)/Common/include
LIBCI   := $(ROOT)/Libc/include
LIBCS   := $(ROOT)/Libc/src

BUILD   := build
OUT     := $(BUILD)/LS.ELF

# ---- Includes ----
INCLUDES := -Iinclude -I$(COMMONI) -I$(LIBCI)

# ---- Flags ----
# Userland: PIE (ET_DYN), freestanding, no libc
CFLAGS  := -target $(TARGET) -std=c11 -O2 -g \
           -ffreestanding -fno-stack-protector \
           -Wall -Wextra -Werror \
           -fpie -mno-red-zone \
           $(INCLUDES)

ASFLAGS := -target $(TARGET) -ffreestanding
LDFLAGS := -nostdlib -pie -e _start

OBJ     := $(BUILD)/crt0.o $(BUILD)/api.o $(BUILD)/ls.o

.PHONY: all clean install

all: $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/crt0.o: $(LIBCS)/crt0.S | $(BUILD)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/api.o: $(LIBCS)/api.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ls.o: ls.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -rf $(BUILD)

# ---- Install into your root FAT partition inside qemu/disk.img ----
# Partition 2 starts at LBA 2048 => byte offset = 2048*512 = 1048576
DISK_IMG   := $(ROOT)/qemu/disk.img
PART_OFF   := 1048576

install: $(OUT)
# mmd -i $(DISK_IMG)@@$(PART_OFF) ::/BIN 2>/dev/null || true
	mcopy -o -i $(DISK_IMG)@@$(PART_OFF) $(OUT) ::/BIN/LS.ELF