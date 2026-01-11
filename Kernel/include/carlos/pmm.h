#pragma once
#include <stdint.h>
#include <carlos/phys.h>
#include <carlos/boot/bootinfo.h>

void     pmm_init(const BootInfo *bi);

uint64_t pmm_alloc_page_phys(void);
void     pmm_free_page_phys(uint64_t phys);

uint64_t pmm_alloc_contig_pages_phys(uint64_t pages);
void     pmm_free_contig_pages_phys(uint64_t base_phys, uint64_t pages);

static inline void* pmm_alloc_page(void) {
  uint64_t phys = pmm_alloc_page_phys();
  return phys ? phys_to_ptr(phys) : 0;
}

static inline void pmm_free_page(void *ptr) {
  if (!ptr) return;
  pmm_free_page_phys(ptr_to_phys(ptr));
}

uint64_t pmm_free_count(void);