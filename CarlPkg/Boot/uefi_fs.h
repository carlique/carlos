#pragma once
#include <Uefi.h>

EFI_STATUS FsReadFileToBuffer(
  IN  EFI_HANDLE   ImageHandle,
  IN  CONST CHAR16 *Path,
  OUT VOID         **Buffer,
  OUT UINTN        *BufferSize
);