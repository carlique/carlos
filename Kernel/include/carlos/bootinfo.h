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

  uint64_t fb_base;
  uint64_t fb_size;
  uint32_t fb_width;
  uint32_t fb_height;
  uint32_t fb_ppsl;
  uint32_t fb_format;

  uint64_t acpi_rsdp;     // physical address
  uint32_t acpi_revision; // 0/1 or 2+
} BootInfo;