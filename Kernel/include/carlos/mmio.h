#pragma once
#include <stdint.h>

static inline uint32_t mmio_read32(uint64_t addr){
  return *(volatile uint32_t*)(uintptr_t)addr;
}
static inline void mmio_write32(uint64_t addr, uint32_t v){
  *(volatile uint32_t*)(uintptr_t)addr = v;
}
static inline uint64_t mmio_read64(uint64_t addr){
  return *(volatile uint64_t*)(uintptr_t)addr;
}
static inline void mmio_write64(uint64_t addr, uint64_t v){
  *(volatile uint64_t*)(uintptr_t)addr = v;
}