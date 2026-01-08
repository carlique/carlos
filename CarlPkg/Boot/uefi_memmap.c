#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h> // gBS, gST
#include <Library/MemoryAllocationLib.h>      // AllocatePool(), FreePool()
#include "uefi_log.h"

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

EFI_STATUS DumpUefiMemoryMap(VOID)
{
  EFI_STATUS Status;

  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
  UINTN MapKey = 0;
  UINTN DescSize = 0;
  UINT32 DescVer = 0;

  // 1) Query required size
  Status = gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescSize, &DescVer);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Logf(L"GetMemoryMap(size query) failed: %r\n", Status);
    return Status;
  }

  // Safety margin: map can grow between calls
  MemMapSize += 2 * DescSize;

  // 2) Allocate buffer and fetch map
  MemMap = (EFI_MEMORY_DESCRIPTOR*)AllocatePool(MemMapSize);
  if (MemMap == NULL) {
    Logf(L"AllocatePool(%lu) failed\n", (UINT64)MemMapSize);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescSize, &DescVer);
  if (EFI_ERROR(Status)) {
    Logf(L"GetMemoryMap(fetch) failed: %r\n", Status);
    FreePool(MemMap);
    return Status;
  }

  Logf(L"UEFI Memory Map: size=%lu descSize=%lu ver=%u key=%lu\n",
       (UINT64)MemMapSize, (UINT64)DescSize, DescVer, (UINT64)MapKey);

  // 3) Iterate using DescSize (not sizeof(EFI_MEMORY_DESCRIPTOR))
  UINTN Count = MemMapSize / DescSize;
  for (UINTN i = 0; i < Count; i++) {
    EFI_MEMORY_DESCRIPTOR *D = (EFI_MEMORY_DESCRIPTOR *)((UINT8*)MemMap + (i * DescSize));

    UINT64 Start = (UINT64)D->PhysicalStart;
    UINT64 SizeBytes = (UINT64)D->NumberOfPages * 4096ULL;
    UINT64 End = (SizeBytes == 0) ? Start : (Start + SizeBytes - 1);

    Logf(L"%03lu %-12s  %016lx - %016lx  pages=%8lu  attr=%016lx\n",
         (UINT64)i,
         MemTypeStr(D->Type),
         Start, End,
         (UINT64)D->NumberOfPages,
         (UINT64)D->Attribute);
  }

  FreePool(MemMap);
  return EFI_SUCCESS;
}
