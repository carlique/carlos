#include <stdint.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/phys.h>
#include <carlos/pmm.h>

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

#define PAGE_SIZE 4096ULL
#define EfiConventionalMemory 7

typedef struct {
  uint32_t Type;
  uint32_t Pad;
  uint64_t PhysicalStart;
  uint64_t VirtualStart;
  uint64_t NumberOfPages;
  uint64_t Attribute;
} EfiMemoryDescriptor;

#define MAX_FREE_PAGES (131072)
static uint64_t g_free_pages[MAX_FREE_PAGES];
static uint64_t g_free_top = 0;

static inline uint64_t align_down(uint64_t x) { return x & ~(PAGE_SIZE - 1); }
static inline uint64_t align_up(uint64_t x)   { return (x + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); }

static inline int in_range(uint64_t x, uint64_t lo, uint64_t hi) {
  return (x >= lo) && (x < hi); // [lo, hi)
}

static inline int is_reserved_page(uint64_t page_phys,
                                  uint64_t kernel_lo, uint64_t kernel_hi,
                                  uint64_t memmap_lo, uint64_t memmap_hi,
                                  uint64_t bi_lo, uint64_t bi_hi)
{
  // page_phys is page-aligned
  if (in_range(page_phys, kernel_lo, kernel_hi)) return 1;
  if (in_range(page_phys, memmap_lo, memmap_hi)) return 1;
  if (in_range(page_phys, bi_lo, bi_hi)) return 1;
  return 0;
}

void pmm_init(const BootInfo *bi)
{
  g_free_top = 0;
  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) return;

  const uint64_t mm_base = bi->memmap;
  const uint64_t mm_size = bi->memmap_size;
  const uint64_t desc_sz = bi->memdesc_size;

  if (!mm_base || !mm_size || !desc_sz) return;

  // Reserve kernel image
  uint64_t kernel_lo = align_down((uint64_t)(uintptr_t)&__kernel_start);
  uint64_t kernel_hi = align_up  ((uint64_t)(uintptr_t)&__kernel_end);

  // Reserve copied UEFI memmap snapshot buffer
  uint64_t memmap_lo = align_down(mm_base);
  uint64_t memmap_hi = align_up(mm_base + mm_size);

  // Reserve BootInfo page (loader allocated 1 page)
  uint64_t bi_phys = bi->bootinfo_phys ? bi->bootinfo_phys : (uint64_t)(uintptr_t)bi;
  uint64_t bi_lo = align_down(bi_phys);
  uint64_t bi_hi = bi_lo + PAGE_SIZE;

  const uint64_t count = mm_size / desc_sz;
  const uint8_t *p = (const uint8_t*)phys_to_cptr(mm_base);

  for (uint64_t i = 0; i < count; i++) {
    const EfiMemoryDescriptor *d =
      (const EfiMemoryDescriptor *)(const void *)(p + i * desc_sz);

    if (d->Type != EfiConventionalMemory) continue;

    uint64_t start = d->PhysicalStart;
    uint64_t pages = d->NumberOfPages;

    for (uint64_t pg = 0; pg < pages; pg++) {
      uint64_t page_phys = align_down(start + pg * PAGE_SIZE);

      if (is_reserved_page(page_phys, kernel_lo, kernel_hi, memmap_lo, memmap_hi, bi_lo, bi_hi))
        continue;

      if (g_free_top < MAX_FREE_PAGES) {
        g_free_pages[g_free_top++] = page_phys;
      }
    }
  }
}

uint64_t pmm_alloc_page_phys(void)
{
  if (g_free_top == 0) return 0;
  return g_free_pages[--g_free_top];   // already physical
}

void pmm_free_page_phys(uint64_t phys)
{
  if (!phys) return;
  phys &= ~(PAGE_SIZE - 1);

  if (g_free_top < MAX_FREE_PAGES) {
    g_free_pages[g_free_top++] = phys;
  }
}

uint64_t pmm_free_count(void)
{
  return g_free_top;
}