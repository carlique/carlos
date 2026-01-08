#pragma once
#include <Uefi.h>

typedef struct BootInfo {
  UINT64 magic;
  UINT64 reserved0;
  UINT64 reserved1;
  UINT64 reserved2;
} BootInfo;

#define CARLOS_BOOTINFO_MAGIC 0x4341524C4F53424FULL  /* "CARLOSBO" */