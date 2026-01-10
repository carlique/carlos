#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>        // CopyMem
#include <Library/MemoryAllocationLib.h>  // FreePool
#include <Protocol/GraphicsOutput.h>
#include <Guid/Acpi.h>

#include <carlos/boot/bootinfo.h>
#include "uefi_log.h"
#include "uefi_memmap.h"
#include "uefi_fs.h"
#include "uefi_elf.h"
#include "uefi_acpi.h"

#define KERNEL_PATH L"\\EFI\\CARLOS\\KERNEL.ELF"

// Raw COM1 for post-exit (same as you already have)
#include <stdint.h>
#define COM1 0x3F8
static inline void outb(uint16_t port, uint8_t val){ __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static void serial_init_raw(void){
  outb(COM1+1,0x00); outb(COM1+3,0x80); outb(COM1+0,0x03); outb(COM1+1,0x00);
  outb(COM1+3,0x03); outb(COM1+2,0xC7); outb(COM1+4,0x0B);
}
static void serial_putc_raw(char c){ while ((inb(COM1+5)&0x20)==0){} outb(COM1,(uint8_t)c); }
static void serial_puts_raw(const char *s){ for(;*s;s++){ if(*s=='\n') serial_putc_raw('\r'); serial_putc_raw(*s);} }

typedef void (*KernelEntry)(BootInfo *bi);

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  LogInit();
  Log(L"CarlBoot: loading kernel...\n");

  // 1) Read kernel ELF from ESP
  VOID *KernelBuf = NULL;
  UINTN KernelSize = 0;
  EFI_STATUS S = FsReadFileToBuffer(ImageHandle, KERNEL_PATH , &KernelBuf, &KernelSize);
  Print(L"FsReadFileToBuffer -> %r (size=%lu)\n", S, (UINT64)KernelSize);
  if (EFI_ERROR(S)) return S;

  // 2) Load PT_LOAD segments
  EFI_PHYSICAL_ADDRESS Entry = 0;
  S = Elf64LoadKernel(KernelBuf, KernelSize, &Entry);
  Print(L"Elf64LoadKernel -> %r entry=%lx\n", S, (UINT64)Entry);
  if (EFI_ERROR(S)) return S;
  
  FreePool(KernelBuf);
  KernelBuf = NULL;
  KernelSize = 0;

  // 3) Allocate BootInfo in a page (so it's easy to reserve later)
  EFI_PHYSICAL_ADDRESS BiAddr = 0;
  S = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &BiAddr);
  if (EFI_ERROR(S)) return S;

  BootInfo *Bi = (BootInfo*)(UINTN)BiAddr;

  // IMPORTANT: wipe the whole struct (pads + future fields)
  ZeroMem(Bi, sizeof(*Bi));

  Bi->magic = CARLOS_BOOTINFO_MAGIC;
  Bi->abi_version = 1;                 // or CARLOS_ABI_VERSION constant
  Bi->bootinfo_phys = (uint64_t)BiAddr; // physical address of BootInfo

  // 4) ExitBootServices using your proven memmap loop
  UEFI_MEMMAP Mm = (UEFI_MEMMAP){0};

  EFI_PHYSICAL_ADDRESS MapCopyAddr = 0;
  UINTN MapCopyPages = 0;

  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
  S = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&Gop);

  if (!EFI_ERROR(S) && Gop && Gop->Mode && Gop->Mode->Info) {
    Bi->fb_base   = (EFI_PHYSICAL_ADDRESS)Gop->Mode->FrameBufferBase;
    Bi->fb_size   = (UINTN)Gop->Mode->FrameBufferSize;
    Bi->fb_width  = (UINT32)Gop->Mode->Info->HorizontalResolution;
    Bi->fb_height = (UINT32)Gop->Mode->Info->VerticalResolution;
    Bi->fb_ppsl   = (UINT32)Gop->Mode->Info->PixelsPerScanLine;
    Bi->fb_format = (UINT32)Gop->Mode->Info->PixelFormat;
  } else {
    Bi->fb_base = 0;
    Bi->fb_size = 0;
    Bi->fb_width = Bi->fb_height = Bi->fb_ppsl = Bi->fb_format = 0;
  }

  Bi->acpi_rsdp = FindAcpiRsdp(SystemTable, &Bi->acpi_guid_kind);

  Bi->rsdp_revision = 0;
  if (Bi->acpi_rsdp) {
    // ACPI 1.0 RSDP layout begins with signature + checksum + OEM + Revision
    typedef struct {
      CHAR8   Signature[8];
      UINT8   Checksum;
      CHAR8   OemId[6];
      UINT8   Revision;
    } RSDP_V1_MIN;

    RSDP_V1_MIN *R = (RSDP_V1_MIN*)(UINTN)Bi->acpi_rsdp;
    Bi->rsdp_revision = R->Revision;
  }

  Print(L"ACPI: RSDP=%lx guid_kind=%u rsdp_rev=%u\n",
        Bi->acpi_rsdp, Bi->acpi_guid_kind, Bi->rsdp_revision);

  while (1) {
    // (1) Acquire map (may allocate pool inside UefiMemMapAcquire if needed)
    S = UefiMemMapAcquire(&Mm);
    if (EFI_ERROR(S)) return S;

    // (2) Ensure snapshot buffer is big enough (AllocatePages changes the map)
    UINTN NeededPages = (Mm.MapSize + 4095) / 4096;
    if (MapCopyAddr == 0 || NeededPages > MapCopyPages) {
      if (MapCopyAddr != 0) {
        gBS->FreePages(MapCopyAddr, MapCopyPages);
        MapCopyAddr = 0;
        MapCopyPages = 0;
      }
      MapCopyPages = NeededPages;
      S = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, MapCopyPages, &MapCopyAddr);
      if (EFI_ERROR(S)) return S;
    }

    // (3) Re-acquire map AFTER any allocations/frees, so MapKey is current
    S = UefiMemMapAcquire(&Mm);
    if (EFI_ERROR(S)) return S;

    // (4) Copy map to snapshot pages (no allocations here)
    CopyMem((VOID*)(UINTN)MapCopyAddr, Mm.Map, Mm.MapSize);

    // (5) Fill BootInfo while still in boot services
    Bi->memmap       = MapCopyAddr;
    Bi->memmap_size  = Mm.MapSize;
    Bi->memdesc_size = Mm.DescSize;
    Bi->memdesc_ver  = Mm.DescVer;

    // (6) ExitBootServices immediately using the fresh MapKey
    S = gBS->ExitBootServices(ImageHandle, Mm.MapKey);
    if (S == EFI_INVALID_PARAMETER) continue;
    break;
  }

  // 5) Post-exit: no UEFI calls. Serial only.
  serial_init_raw();
  serial_puts_raw("CarlBoot: ExitBootServices OK, jumping to kernel...\n");

  // Optional: disable interrupts before handing off
  __asm__ volatile ("cli");

  ((KernelEntry)(UINTN)Entry)(Bi);

  // If kernel returns, halt.
  serial_puts_raw("CarlBoot: kernel returned (unexpected)\n");
  for (;;) { __asm__ volatile ("hlt"); }
}