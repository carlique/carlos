#include <stddef.h>
#include <stdint.h>
#include <carlos/kmem.h>
#include <carlos/pmm.h>
#include <carlos/phys.h>

#define PAGE_SIZE 4096ULL

// Simple bump-pointer kernel heap allocator (not thread-safe, no freeing yet)
static uint64_t heap_phys_cur = 0;
static uint64_t heap_phys_end = 0;

// align x up to multiple of a
static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
  return (x + a - 1) & ~(a - 1);
}

// Initialize kernel memory allocator
void kmem_init(void) {
  heap_phys_cur = 0;
  heap_phys_end = 0;
}

/* 
  Allocate 'size' bytes from kernel heap
  param size Number of bytes to allocate
  return Pointer to allocated memory, or 0 on failure
 */
void* kmalloc(size_t size) {
  if (size == 0) return 0;

  // For now: keep kmalloc "small"; no multi-page objects yet.
  if (size > (PAGE_SIZE - 32)) return 0;

  // Need a page / new extent?
  if (heap_phys_cur == 0 || align_up_u64(heap_phys_cur, 16) + size > heap_phys_end) {
    uint64_t page_phys = pmm_alloc_page_phys();
    if (!page_phys) return 0;

    heap_phys_cur = page_phys;
    heap_phys_end = page_phys + PAGE_SIZE;
  }

  uint64_t cur = align_up_u64(heap_phys_cur, 16);
  uint64_t next = cur + (uint64_t)size;

  heap_phys_cur = next;
  return phys_to_ptr(cur);
}

/* 
    Free previously allocated memory (no-op for now)
    param p Pointer to memory to free
 */
void kfree(void *p) {
  (void)p; // no-op for now
}