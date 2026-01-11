#include <stddef.h>
#include <stdint.h>
#include <carlos/kmem.h>
#include <carlos/pmm.h>
#include <carlos/phys.h>
#include <carlos/klog.h>

#define PAGE_SIZE 4096ULL

typedef struct {
  uint64_t pages;  // 0 = small bump alloc, >0 = contig alloc pages
  uint64_t size;   // requested size
} KAllocHdr;

static uint64_t heap_phys_cur = 0;
static uint64_t heap_phys_end = 0;

static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
  return (x + a - 1) & ~(a - 1);
}

void kmem_init(void) {
  heap_phys_cur = 0;
  heap_phys_end = 0;
}

void* kmalloc(size_t size) {
  if (size == 0) return 0;

  kprintf("kmalloc: size=%u\n", (unsigned)size);
  const uint64_t hdr_sz = align_up_u64(sizeof(KAllocHdr), 16);
  const uint64_t total  = (uint64_t)size + hdr_sz;

  // Small path: keep original bump style within one page.
  // We cap to leave space for header and a little slack.
  const uint64_t small_max = PAGE_SIZE - 32;
  if (total <= small_max) {
    if (heap_phys_cur == 0 || align_up_u64(heap_phys_cur, 16) + total > heap_phys_end) {
      uint64_t page_phys = pmm_alloc_page_phys();
      if (!page_phys) return 0;
      heap_phys_cur = page_phys;
      heap_phys_end = page_phys + PAGE_SIZE;
    }

    uint64_t cur = align_up_u64(heap_phys_cur, 16);
    uint64_t next = cur + total;
    heap_phys_cur = next;

    KAllocHdr *h = (KAllocHdr*)(uintptr_t)cur;
    h->pages = 0;              // small bump alloc
    h->size  = (uint64_t)size;

    return (void*)(uintptr_t)(cur + hdr_sz);
  }

  kprintf("kmalloc BIG: size=%u\n", (unsigned)size);
  // Big path: allocate contiguous pages
  uint64_t pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
  uint64_t base_phys = pmm_alloc_contig_pages_phys(pages);
  if (!base_phys) return 0;

  KAllocHdr *h = (KAllocHdr*)(uintptr_t)base_phys;
  h->pages = pages;
  h->size  = (uint64_t)size;

  return (void*)(uintptr_t)(base_phys + hdr_sz);
}

void kfree(void *p) {
  if (!p) return;

  const uint64_t hdr_sz = align_up_u64(sizeof(KAllocHdr), 16);
  uint64_t addr = (uint64_t)(uintptr_t)p;
  uint64_t hdr_addr = addr - hdr_sz;

  KAllocHdr *h = (KAllocHdr*)(uintptr_t)hdr_addr;

  // Only free big allocations (contiguous pages). Small bump allocs stay no-op for now.
  if (h->pages > 0) {
    uint64_t base_phys = hdr_addr & ~(PAGE_SIZE - 1); // header placed at base
    pmm_free_contig_pages_phys(base_phys, h->pages);
  }
}