#pragma once
#include <stdint.h>
#include <stddef.h>

// For now, kernel is effectively identity-mapped for these regions in QEMU.
// Later this will create a proper virtual mapping (UC/WC attributes).
static inline void* iomap(uint64_t phys, size_t size, uint32_t flags){
  (void)size; (void)flags;
  return (void*)(uintptr_t)phys;
}