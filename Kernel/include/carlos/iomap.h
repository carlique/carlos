#pragma once
#include <stdint.h>
#include <stddef.h>
#include <carlos/phys.h>

// For now, kernel is effectively identity-mapped for these regions in QEMU.
// Later this will create a proper virtual mapping (UC/WC attributes).
static inline void* iomap(uint64_t phys, size_t size, uint32_t flags){
  (void)size; (void)flags;
  return phys_to_ptr(phys);
}