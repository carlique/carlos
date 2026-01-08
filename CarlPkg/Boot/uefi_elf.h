#pragma once
#include <Uefi.h>

EFI_STATUS Elf64LoadKernel(
  IN  CONST VOID  *ElfImage,
  IN  UINTN       ElfSize,
  OUT EFI_PHYSICAL_ADDRESS *EntryPoint
);