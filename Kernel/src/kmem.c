#include <stddef.h>
#include <stdint.h>
#include <carlos/kmem.h>
#include <carlos/pmm.h>

#define PAGE_SIZE 4096ULL

static uint8_t *heap_cur = 0;
static uint8_t *heap_end = 0;

static inline uintptr_t align_up(uintptr_t x, uintptr_t a) {
  return (x + a - 1) & ~(a - 1);
}

void kmem_init(void) {
  heap_cur = 0;
  heap_end = 0;
}

void* kmalloc(size_t size) {
  if (size == 0) return 0;

  // align allocations (16 is a good baseline for x86_64)
  uintptr_t cur = (uintptr_t)heap_cur;
  cur = align_up(cur, 16);
  uintptr_t need_end = cur + size;

  // Ensure we have space: allocate pages as needed
  while (heap_cur == 0 || need_end > (uintptr_t)heap_end) {
    void *page = pmm_alloc_page();
    if (!page) return 0;

    // First page initializes the heap window
    if (heap_cur == 0) {
      heap_cur = (uint8_t*)page;
      heap_end = (uint8_t*)page + PAGE_SIZE;
      cur = align_up((uintptr_t)heap_cur, 16);
      need_end = cur + size;
    } else {
      // Require pages to be contiguous for this simple bump heap
      if ((uint8_t*)page != heap_end) {
        // Not contiguous: put it back and fail (for now)
        pmm_free_page(page);
        return 0;
      }
      heap_end += PAGE_SIZE;
    }
  }

  void *ret = (void*)cur;
  heap_cur = (uint8_t*)need_end;
  return ret;
}

void kfree(void *p) {
  (void)p; // no-op for now
}