#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096ULL

static inline void* phys_to_ptr(uint64_t phys){
  // identity-map for now
  return (void*)(uintptr_t)phys;
}

static inline const void* phys_to_cptr(uint64_t phys){
  // identity-map for now
  return (const void*)(uintptr_t)phys;
}

static inline uint64_t ptr_to_phys(const void *p){
  // identity-map for now
  return (uint64_t)(uintptr_t)p;
}

static inline uint64_t page_align_down(uint64_t x){
  return x & ~(PAGE_SIZE - 1);
}

static inline uint64_t page_align_up(uint64_t x){
  return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}