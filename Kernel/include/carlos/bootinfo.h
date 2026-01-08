#pragma once
#include <stdint.h>

typedef struct BootInfo {
  uint64_t magic;
  uint64_t r0, r1, r2;
} BootInfo;

#define CARLOS_BOOTINFO_MAGIC 0x4341524C4F53424FULL /* "CARLOSBO" */