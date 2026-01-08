#include <Uefi.h>
#include "uefi_memmap.h"

#include <Library/UefiBootServicesTableLib.h> // gBS
#include <Library/MemoryAllocationLib.h>      // AllocatePool, FreePool
#include <Library/UefiLib.h>                  // Print

static CONST CHAR16* MemTypeStr(EFI_MEMORY_TYPE T)
{
  switch (T) {
    case EfiReservedMemoryType:       return L"Reserved";
    case EfiLoaderCode:               return L"LoaderCode";
    case EfiLoaderData:               return L"LoaderData";
    case EfiBootServicesCode:         return L"BootSvcCode";
    case EfiBootServicesData:         return L"BootSvcData";
    case EfiRuntimeServicesCode:      return L"RunSvcCode";
    case EfiRuntimeServicesData:      return L"RunSvcData";
    case EfiConventionalMemory:       return L"Conventional";
    case EfiUnusableMemory:           return L"Unusable";
    case EfiACPIReclaimMemory:        return L"ACPIReclaim";
    case EfiACPIMemoryNVS:            return L"ACPINvs";
    case EfiMemoryMappedIO:           return L"MMIO";
    case EfiMemoryMappedIOPortSpace:  return L"MMIOPort";
    case EfiPalCode:                  return L"PalCode";
    case EfiPersistentMemory:         return L"Persistent";
    default:                          return L"Unknown";
  }
}

EFI_STATUS UefiMemMapAcquire(UEFI_MEMMAP *Mm)
{
  if (Mm == NULL) return EFI_INVALID_PARAMETER;

  EFI_STATUS Status;

  // If no buffer yet, do the "size query" to know what to allocate.
  if (Mm->Map == NULL || Mm->MapCapacity == 0) {
    UINTN  Size = 0;
    UINTN  Key  = 0;
    UINTN  Dsz  = 0;
    UINT32 Dver = 0;

    Status = gBS->GetMemoryMap(&Size, NULL, &Key, &Dsz, &Dver);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      return Status;
    }

    // Safety margin: map can grow between calls.
    Size += 8 * Dsz;

    Mm->Map = (EFI_MEMORY_DESCRIPTOR*)AllocatePool(Size);
    if (Mm->Map == NULL) return EFI_OUT_OF_RESOURCES;

    Mm->MapCapacity = Size;
  }

  // Fetch into existing buffer. If too small, reallocate and retry.
  while (1) {
    Mm->MapSize = Mm->MapCapacity;

    Status = gBS->GetMemoryMap(
      &Mm->MapSize,
      Mm->Map,
      &Mm->MapKey,
      &Mm->DescSize,
      &Mm->DescVer
    );

    if (Status == EFI_BUFFER_TOO_SMALL) {
      // Realloc bigger and retry
      FreePool(Mm->Map);
      Mm->Map = NULL;
      Mm->MapCapacity = 0;
      return UefiMemMapAcquire(Mm);
    }

    return Status;
  }
}

VOID UefiMemMapPrint(CONST UEFI_MEMMAP *Mm)
{
  if (Mm == NULL || Mm->Map == NULL) return;

  Print(L"UEFI Memory Map: size=%lu descSize=%lu ver=%u key=%lu\n",
        (UINT64)Mm->MapSize, (UINT64)Mm->DescSize, Mm->DescVer, (UINT64)Mm->MapKey);

  UINTN Count = Mm->MapSize / Mm->DescSize;
  for (UINTN i = 0; i < Count; i++) {
    EFI_MEMORY_DESCRIPTOR *D =
      (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Mm->Map + (i * Mm->DescSize));

    UINT64 Start = (UINT64)D->PhysicalStart;
    UINT64 SizeBytes = (UINT64)D->NumberOfPages * 4096ULL;
    UINT64 End = (SizeBytes == 0) ? Start : (Start + SizeBytes - 1);

    Print(L"%03lu %-12s  %016lx - %016lx  pages=%8lu  attr=%016lx\n",
          (UINT64)i,
          MemTypeStr(D->Type),
          Start, End,
          (UINT64)D->NumberOfPages,
          (UINT64)D->Attribute);
  }
}