#pragma once
#include <stdint.h>
#include <carlos/bootinfo.h>

void pmm_init(const BootInfo *bi);

// returns physical address (identity-mapped for now)
void* pmm_alloc_page(void);
void  pmm_free_page(void *p);

// stats
uint64_t pmm_free_count(void);