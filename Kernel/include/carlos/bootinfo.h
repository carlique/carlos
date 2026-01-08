#pragma once
#include <stdint.h>

#define CARLOS_BOOTINFO_MAGIC 0x4341524C4F53424FULL

typedef struct BootInfo {
  uint64_t magic;

  uint64_t bootinfo;
  
  uint64_t memmap;
  uint64_t memmap_size;
  uint64_t memdesc_size;
  uint32_t memdesc_ver;
} BootInfo;