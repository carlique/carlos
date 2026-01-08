#include <Uefi.h>

#include <Library/BaseMemoryLib.h>          // CopyMem, SetMem
#include <Library/MemoryAllocationLib.h>   // AllocatePages
#include <Library/UefiLib.h>               // Print
#include <Library/UefiBootServicesTableLib.h> // gBS

#include "uefi_elf.h"

#define EI_NIDENT 16
#define PT_LOAD   1
#define EM_X86_64 62
#define ELFCLASS64 2
#define ELFDATA2LSB 1

typedef struct {
  UINT8  e_ident[EI_NIDENT];
  UINT16 e_type;
  UINT16 e_machine;
  UINT32 e_version;
  UINT64 e_entry;
  UINT64 e_phoff;
  UINT64 e_shoff;
  UINT32 e_flags;
  UINT16 e_ehsize;
  UINT16 e_phentsize;
  UINT16 e_phnum;
  UINT16 e_shentsize;
  UINT16 e_shnum;
  UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
  UINT32 p_type;
  UINT32 p_flags;
  UINT64 p_offset;
  UINT64 p_vaddr;
  UINT64 p_paddr;
  UINT64 p_filesz;
  UINT64 p_memsz;
  UINT64 p_align;
} Elf64_Phdr;

static BOOLEAN InBounds(UINTN Off, UINTN Size, UINTN Limit) {
  return (Off <= Limit) && (Size <= Limit - Off);
}

EFI_STATUS Elf64LoadKernel(
  IN  CONST VOID  *ElfImage,
  IN  UINTN       ElfSize,
  OUT EFI_PHYSICAL_ADDRESS *EntryPoint
)
{
  if (!ElfImage || !EntryPoint) return EFI_INVALID_PARAMETER;
  *EntryPoint = 0;

  if (ElfSize < sizeof(Elf64_Ehdr)) return EFI_LOAD_ERROR;

  const UINT8 *Base = (const UINT8*)ElfImage;
  const Elf64_Ehdr *Eh = (const Elf64_Ehdr*)ElfImage;

  // Magic
  if (!(Eh->e_ident[0] == 0x7F && Eh->e_ident[1] == 'E' && Eh->e_ident[2] == 'L' && Eh->e_ident[3] == 'F'))
    return EFI_LOAD_ERROR;
  if (Eh->e_ident[4] != ELFCLASS64) return EFI_UNSUPPORTED;
  if (Eh->e_ident[5] != ELFDATA2LSB) return EFI_UNSUPPORTED;
  if (Eh->e_machine != EM_X86_64) return EFI_UNSUPPORTED;
  // Require fixed-address executable for now (no relocations/PIE yet)
  if (Eh->e_type != 2 /* ET_EXEC */) {
    Print(L"ELF type not ET_EXEC (got %u)\n", Eh->e_type);
    return EFI_UNSUPPORTED;
  }

  // Program header table bounds
  UINTN PhOff = (UINTN)Eh->e_phoff;
  UINTN PhEnt = (UINTN)Eh->e_phentsize;
  UINTN PhNum = (UINTN)Eh->e_phnum;
  if (PhEnt != sizeof(Elf64_Phdr)) return EFI_UNSUPPORTED;
  if (!InBounds(PhOff, PhEnt * PhNum, ElfSize)) return EFI_LOAD_ERROR;

  const Elf64_Phdr *Ph = (const Elf64_Phdr*)(Base + PhOff);

  // First pass: compute total load range across all PT_LOAD
  EFI_PHYSICAL_ADDRESS Min = (EFI_PHYSICAL_ADDRESS)(~0ULL);
  EFI_PHYSICAL_ADDRESS Max = 0;
  BOOLEAN Found = FALSE;

  for (UINTN i = 0; i < PhNum; i++) {
    if (Ph[i].p_type != PT_LOAD) continue;
    if (Ph[i].p_memsz == 0) continue;

    if (Ph[i].p_filesz > Ph[i].p_memsz) return EFI_LOAD_ERROR;
    if (Ph[i].p_type == PT_LOAD && Ph[i].p_filesz > 0) {
      UINTN Off = (UINTN)Ph[i].p_offset;
      UINTN Sz  = (UINTN)Ph[i].p_filesz;
      if (!InBounds(Off, Sz, ElfSize)) return EFI_LOAD_ERROR;
    }

    EFI_PHYSICAL_ADDRESS Seg = (EFI_PHYSICAL_ADDRESS)(Ph[i].p_paddr ? Ph[i].p_paddr : Ph[i].p_vaddr);
    EFI_PHYSICAL_ADDRESS SegEnd = Seg + (EFI_PHYSICAL_ADDRESS)Ph[i].p_memsz;

    if (Seg < Min) Min = Seg;
    if (SegEnd > Max) Max = SegEnd;

    if ((EFI_PHYSICAL_ADDRESS)Eh->e_entry < Min || (EFI_PHYSICAL_ADDRESS)Eh->e_entry >= Max) {
      Print(L"Entry point %lx outside loaded range [%lx, %lx)\n",
            (UINT64)Eh->e_entry, (UINT64)Min, (UINT64)Max);
      return EFI_LOAD_ERROR;
    }

    Found = TRUE;
  }

  if (!Found) return EFI_LOAD_ERROR;

  // Align to pages and allocate once
  EFI_PHYSICAL_ADDRESS PageMin = Min & ~((EFI_PHYSICAL_ADDRESS)0xFFF);
  EFI_PHYSICAL_ADDRESS PageMax = (Max + 0xFFF) & ~((EFI_PHYSICAL_ADDRESS)0xFFF);
  UINTN Pages = (UINTN)((PageMax - PageMin) / 4096ULL);

  EFI_PHYSICAL_ADDRESS AllocAddr = PageMin;
  EFI_STATUS Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, Pages, &AllocAddr);
  if (EFI_ERROR(Status)) {
    Print(L"AllocatePages @%lx pages=%lu failed: %r\n", (UINT64)AllocAddr, (UINT64)Pages, Status);
    return Status;
  }

  // Zero the whole region
  SetMem((VOID*)(UINTN)PageMin, Pages * 4096ULL, 0);

  // Second pass: copy segments
  for (UINTN i = 0; i < PhNum; i++) {
    if (Ph[i].p_type != PT_LOAD) continue;

    if (Ph[i].p_filesz > 0) {
      UINTN Off = (UINTN)Ph[i].p_offset;
      UINTN Sz  = (UINTN)Ph[i].p_filesz;
      if (!InBounds(Off, Sz, ElfSize)) return EFI_LOAD_ERROR;

      EFI_PHYSICAL_ADDRESS Seg = (EFI_PHYSICAL_ADDRESS)(Ph[i].p_paddr ? Ph[i].p_paddr : Ph[i].p_vaddr);
      CopyMem((VOID*)(UINTN)Seg, Base + Off, Sz);
    }
  }

  *EntryPoint = (EFI_PHYSICAL_ADDRESS)Eh->e_entry;
  return EFI_SUCCESS;
}