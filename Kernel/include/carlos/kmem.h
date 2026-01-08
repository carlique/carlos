#pragma once
#include <stddef.h>
#include <stdint.h>

void  kmem_init(void);
void* kmalloc(size_t size);
void  kfree(void *p);   // stub for now