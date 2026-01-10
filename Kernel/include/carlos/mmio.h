#pragma once
#include <stdint.h>
#include <carlos/phys.h>

// Physical MMIO helpers (phys addr -> mapped pointer via phys_to_ptr)

static inline uint8_t mmio_read8_phys(uint64_t phys){
  return *(volatile uint8_t*)phys_to_ptr(phys);
}
static inline void mmio_write8_phys(uint64_t phys, uint8_t v){
  *(volatile uint8_t*)phys_to_ptr(phys) = v;
}

static inline uint16_t mmio_read16_phys(uint64_t phys){
  return *(volatile uint16_t*)phys_to_ptr(phys);
}
static inline void mmio_write16_phys(uint64_t phys, uint16_t v){
  *(volatile uint16_t*)phys_to_ptr(phys) = v;
}

static inline uint32_t mmio_read32_phys(uint64_t phys){
  return *(volatile uint32_t*)phys_to_ptr(phys);
}
static inline void mmio_write32_phys(uint64_t phys, uint32_t v){
  *(volatile uint32_t*)phys_to_ptr(phys) = v;
}

static inline uint64_t mmio_read64_phys(uint64_t phys){
  return *(volatile uint64_t*)phys_to_ptr(phys);
}
static inline void mmio_write64_phys(uint64_t phys, uint64_t v){
  *(volatile uint64_t*)phys_to_ptr(phys) = v;
}