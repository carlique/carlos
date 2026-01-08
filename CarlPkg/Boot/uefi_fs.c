#include <Uefi.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include "uefi_fs.h"

EFI_STATUS FsReadFileToBuffer(
  IN  EFI_HANDLE   ImageHandle,
  IN  CONST CHAR16 *Path,
  OUT VOID         **Buffer,
  OUT UINTN        *BufferSize
)
{
  if (!Path || !Buffer || !BufferSize) return EFI_INVALID_PARAMETER;
  *Buffer = NULL;
  *BufferSize = 0;

  EFI_STATUS Status;

  EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
  Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&Loaded);
  if (EFI_ERROR(Status)) return Status;

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs = NULL;
  Status = gBS->HandleProtocol(Loaded->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Sfs);
  if (EFI_ERROR(Status)) return Status;

  EFI_FILE_PROTOCOL *Root = NULL;
  Status = Sfs->OpenVolume(Sfs, &Root);
  if (EFI_ERROR(Status)) return Status;

  EFI_FILE_PROTOCOL *File = NULL;
  Status = Root->Open(Root, &File, (CHAR16*)Path, EFI_FILE_MODE_READ, 0);
  Root->Close(Root);
  if (EFI_ERROR(Status)) return Status;

  // Query file size
  UINTN InfoSize = 0;
  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    File->Close(File);
    return Status;
  }

  EFI_FILE_INFO *Info = (EFI_FILE_INFO*)AllocatePool(InfoSize);
  if (!Info) {
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, Info);
  if (EFI_ERROR(Status)) {
    FreePool(Info);
    File->Close(File);
    return Status;
  }

  UINTN Size = (UINTN)Info->FileSize;
  FreePool(Info);

  VOID *Buf = AllocatePool(Size);
  if (!Buf) {
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  UINTN ReadSize = Size;
  Status = File->Read(File, &ReadSize, Buf);
  File->Close(File);

  if (EFI_ERROR(Status) || ReadSize != Size) {
    FreePool(Buf);
    return EFI_DEVICE_ERROR;
  }

  *Buffer = Buf;
  *BufferSize = Size;
  return EFI_SUCCESS;
}