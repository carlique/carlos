#pragma once
#include <Uefi.h>

typedef struct {
  EFI_MEMORY_DESCRIPTOR *Map;
  UINTN                  MapSize;      // bytes actually returned
  UINTN                  MapCapacity;  // bytes allocated
  UINTN                  MapKey;
  UINTN                  DescSize;
  UINT32                 DescVer;
} UEFI_MEMMAP;

EFI_STATUS UefiMemMapAcquire(UEFI_MEMMAP *Mm);
VOID      UefiMemMapPrint  (CONST UEFI_MEMMAP *Mm);