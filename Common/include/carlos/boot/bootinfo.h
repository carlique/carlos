#pragma once
#include <stdint.h>

// Pick one offset macro depending on environment
#if defined(EFIAPI) || defined(MDE_CPU_X64) || defined(MDEPKG_NDEBUG) || defined(__EDKII__)
  #include <Base.h>          // OFFSET_OF, STATIC_ASSERT, etc.
  #define CARLOS_OFFSETOF(T, F) OFFSET_OF(T, F)
#else
  #include <stddef.h>        // offsetof
  #define CARLOS_OFFSETOF(T, F) offsetof(T, F)
#endif

#define CARLOS_BOOTINFO_MAGIC 0x4341524C4F53424FULL  // "CARLOSBO" (example)

#define CARLOS_PATH_MAX  128
#define CARLOS_ROOT_MAX  128

typedef struct BootInfo {
  uint64_t magic;
  uint32_t abi_version;     // start with 1
  uint32_t bootinfo_size;   // sizeof(BootInfo) written by loader
  uint32_t _pad_magic;

  // Physical address of this BootInfo struct (stable anchor after paging changes)
  uint64_t bootinfo_phys;

  // UEFI memory map copy (physical)
  uint64_t memmap;          // phys addr of copied descriptors
  uint64_t memmap_size;     // bytes valid
  uint64_t memdesc_size;    // stride
  uint32_t memdesc_ver;     // version
  uint32_t _pad0;

  // GOP framebuffer (physical)
  uint64_t fb_base;
  uint64_t fb_size;
  uint32_t fb_width;
  uint32_t fb_height;
  uint32_t fb_ppsl;         // pixels per scanline
  uint32_t fb_format;       // EFI_GRAPHICS_PIXEL_FORMAT

  // ACPI RSDP (physical)
  uint64_t acpi_rsdp;
  uint32_t acpi_guid_kind;  // 1 = ACPI1.0 GUID, 2 = ACPI2.0 GUID
  uint8_t  rsdp_revision;   // 0 (v1), 2+ (v2+)
  uint8_t  _pad8[3];

  // Carlos-specific extensions
  char kernel_path[CARLOS_PATH_MAX]; // e.g. "\EFI\CARLOS\KERNEL.ELF"
  char root_spec[CARLOS_ROOT_MAX];   // e.g. "partuuid=...." or "esp"

} BootInfo;

_Static_assert(sizeof(BootInfo) == 368, "BootInfo size changed");
_Static_assert(sizeof(BootInfo) % 8 == 0, "BootInfo must be 8-byte aligned");

// Layout guards (so you notice accidental edits immediately)
_Static_assert(CARLOS_OFFSETOF(BootInfo, memmap)      == 32,  "BootInfo layout changed");
_Static_assert(CARLOS_OFFSETOF(BootInfo, fb_base)     == 64,  "BootInfo layout changed");
_Static_assert(CARLOS_OFFSETOF(BootInfo, acpi_rsdp)   == 96,  "BootInfo layout changed");
_Static_assert(CARLOS_OFFSETOF(BootInfo, kernel_path) == 112, "BootInfo layout changed");
_Static_assert(CARLOS_OFFSETOF(BootInfo, root_spec)   == 240, "BootInfo layout changed");