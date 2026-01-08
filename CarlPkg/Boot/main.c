#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

#include "uefi_log.h"
#include "uefi_memmap.h"
#include "uefi_fs.h"
#include "uefi_elf.h"
#include "bootinfo.h"

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
  EFI_STATUS S = FsReadFileToBuffer(ImageHandle, L"\\EFI\\CARLOS\\KERNEL.ELF", &KernelBuf, &KernelSize);
  Print(L"FsReadFileToBuffer -> %r (size=%lu)\n", S, (UINT64)KernelSize);
  if (EFI_ERROR(S)) return S;

  // 2) Load PT_LOAD segments
  EFI_PHYSICAL_ADDRESS Entry = 0;
  S = Elf64LoadKernel(KernelBuf, KernelSize, &Entry);
  Print(L"Elf64LoadKernel -> %r entry=%lx\n", S, (UINT64)Entry);
  if (EFI_ERROR(S)) return S;

  // 3) Allocate BootInfo in a page (so it's easy to reserve later)
  EFI_PHYSICAL_ADDRESS BiAddr = 0;
  S = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &BiAddr);
  if (EFI_ERROR(S)) return S;

  BootInfo *Bi = (BootInfo*)(UINTN)BiAddr;
  Bi->magic = CARLOS_BOOTINFO_MAGIC;

  // 4) ExitBootServices using your proven memmap loop
  UEFI_MEMMAP Mm = {0};

  while (1) {
    S = UefiMemMapAcquire(&Mm);
    if (EFI_ERROR(S)) return S;

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