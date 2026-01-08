#pragma once
#include <Uefi.h>

#define CARLOS_BOOTINFO_MAGIC 0x4341524C4F53424FULL  /* "CARLOSBO" */

typedef struct BootInfo {
  UINT64 magic;

  // Snapshot taken immediately before ExitBootServices
  EFI_PHYSICAL_ADDRESS bootinfo;
  
  EFI_PHYSICAL_ADDRESS memmap;       // points to copied descriptors (pages)
  UINTN                memmap_size;  // bytes valid
  UINTN                memdesc_size; // stride
  UINT32               memdesc_ver;
} BootInfo;