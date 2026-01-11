#include <stddef.h>
#include <stdint.h>
#include <carlos/kmem.h>
#include <carlos/pmm.h>
#include <carlos/phys.h>
#include <carlos/klog.h>

#define PAGE_SIZE 4096ULL

// Simple bump-pointer kernel heap allocator (small allocs)
// (not thread-safe, no freeing for small allocs yet)
static uint64_t heap_phys_cur = 0;
static uint64_t heap_phys_end = 0;

static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
  return (x + a - 1) & ~(a - 1);
}

void kmem_init(void) {
  heap_phys_cur = 0;
  heap_phys_end = 0;
}

/* ---------------- BIG allocations (page-backed, freeable) ---------------- */

#define KMALLOC_BIG_MAGIC 0x4B4D414C554C4C21ull /* "KMALLUL!" (any odd 64b is fine) */

typedef struct __attribute__((packed)) {
  uint64_t magic;
  uint32_t pages;
  uint32_t _pad0;
  uint64_t base_phys;   // page-aligned physical base returned by PMM
  uint64_t _pad1;       // keep header 32B
} KmallocBigHdr;

static void* kmalloc_big(size_t size) {
  uint64_t total = (uint64_t)size + (uint64_t)sizeof(KmallocBigHdr);
  uint64_t pages = (total + (PAGE_SIZE - 1)) / PAGE_SIZE;
  if (pages == 0) return 0;

  uint64_t base_phys = pmm_alloc_contig_pages_phys(pages);
  if (!base_phys) return 0;

  uint8_t *base = (uint8_t*)phys_to_ptr(base_phys);

  KmallocBigHdr *h = (KmallocBigHdr*)base;
  h->magic    = KMALLOC_BIG_MAGIC;
  h->pages    = (uint32_t)pages;
  h->_pad0    = 0;
  h->base_phys= base_phys;
  h->_pad1    = 0;

  return (void*)(base + sizeof(KmallocBigHdr));
}

/* ---------------- kmalloc ---------------- */

void* kmalloc(size_t size) {
  if (size == 0) return 0;

  // BIG allocation: go to PMM contiguous allocator, and make it freeable
  if (size > (PAGE_SIZE - 32)) {
    // optional debug (you already had something similar)
    // kprintf("kmalloc BIG: size=%u\n", (unsigned)size);
    return kmalloc_big(size);
  }

  // SMALL allocation: bump within a single page (no free yet)
  if (heap_phys_cur == 0 || align_up_u64(heap_phys_cur, 16) + size > heap_phys_end) {
    uint64_t page_phys = pmm_alloc_page_phys();
    if (!page_phys) return 0;

    heap_phys_cur = page_phys;
    heap_phys_end = page_phys + PAGE_SIZE;
  }

  uint64_t cur  = align_up_u64(heap_phys_cur, 16);
  uint64_t next = cur + (uint64_t)size;

  heap_phys_cur = next;
  return phys_to_ptr(cur);
}

/* ---------------- kfree ---------------- */

void kfree(void *p) {
  if (!p) return;

  uint8_t *u = (uint8_t*)p;
  KmallocBigHdr *h = (KmallocBigHdr*)(u - sizeof(KmallocBigHdr));

  if (h->magic != KMALLOC_BIG_MAGIC) return;
  if (h->pages == 0) return;
  if (h->base_phys == 0) return;

  pmm_free_contig_pages_phys(h->base_phys, (uint64_t)h->pages);
}