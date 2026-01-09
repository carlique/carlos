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

  // GOP framebuffer (valid if fb_base != 0)
  EFI_PHYSICAL_ADDRESS fb_base;
  UINTN                fb_size;
  UINT32               fb_width;
  UINT32               fb_height;
  UINT32               fb_ppsl;     // pixels per scanline (pitch in pixels)
  UINT32               fb_format;   // EFI_GRAPHICS_PIXEL_FORMAT

  EFI_PHYSICAL_ADDRESS acpi_rsdp;     // physical address
  UINT32               acpi_revision; // 0/1 or 2+
} BootInfo;