#include <stdint.h>
#include <carlos/bootinfo.h>
#include <carlos/pmm.h>

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

#define PAGE_SIZE 4096ULL

// UEFI memory type values (we only care about Conventional=7)
#define EfiConventionalMemory 7

typedef struct {
  uint32_t Type;
  uint32_t Pad;
  uint64_t PhysicalStart;
  uint64_t VirtualStart;
  uint64_t NumberOfPages;
  uint64_t Attribute;
} EfiMemoryDescriptor;

// A simple stack of free pages.
// Adjust this if you want to support very large RAM.
// 131072 * 8 = 1 MiB metadata
#define MAX_FREE_PAGES (131072)

static uint64_t g_free_pages[MAX_FREE_PAGES];
static uint64_t g_free_top = 0;

static inline uint64_t align_up(uint64_t x, uint64_t a) {
  return (x + a - 1) & ~(a - 1);
}
static inline uint64_t align_down(uint64_t x, uint64_t a) {
  return x & ~(a - 1);
}

// returns 1 if [p, p+PAGE_SIZE) intersects [a, b)
static int page_intersects(uint64_t p, uint64_t a, uint64_t b) {
  uint64_t pe = p + PAGE_SIZE;
  return !(pe <= a || p >= b);
}

static int is_reserved(uint64_t page_addr,
                       uint64_t k_start, uint64_t k_end,
                       uint64_t mm_start, uint64_t mm_end,
                       uint64_t bi_addr)
{
  // reserve kernel image range
  if (page_intersects(page_addr, k_start, k_end)) return 1;
  // reserve memmap snapshot range
  if (page_intersects(page_addr, mm_start, mm_end)) return 1;
  // reserve BootInfo page (bi_addr is one page)
  if (page_addr == (bi_addr & ~(PAGE_SIZE - 1))) return 1;
  return 0;
}

void pmm_init(const BootInfo *bi)
{
  g_free_top = 0;
  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) return;

  // These are true “physical” addresses at this stage (identity mapping assumption)
  uint64_t mm_base = bi->memmap;
  uint64_t mm_size = bi->memmap_size;
  uint64_t desc_sz = bi->memdesc_size;

  // Reserve ranges:
  uint64_t kernel_start = (uint64_t)(uintptr_t)&__kernel_start;
  uint64_t kernel_end   = (uint64_t)(uintptr_t)&__kernel_end;
  kernel_start = align_down(kernel_start, PAGE_SIZE);
  kernel_end   = align_up(kernel_end, PAGE_SIZE);

  uint64_t memmap_start = align_down(mm_base, PAGE_SIZE);
  uint64_t memmap_end   = align_up(mm_base + mm_size, PAGE_SIZE);

  // BootInfo struct location
  uint64_t bootinfo_page = (uint64_t)bi->bootinfo;

  uint64_t count = (desc_sz == 0) ? 0 : (mm_size / desc_sz);
  const uint8_t *p = (const uint8_t *)(uintptr_t)mm_base;

  for (uint64_t i = 0; i < count; i++) {
    const EfiMemoryDescriptor *d = (const EfiMemoryDescriptor *)(const void *)(p + i * desc_sz);

    if (d->Type != EfiConventionalMemory) continue;

    uint64_t start = d->PhysicalStart;
    uint64_t pages = d->NumberOfPages;

    for (uint64_t pg = 0; pg < pages; pg++) {
      uint64_t addr = start + pg * PAGE_SIZE;

      if (is_reserved(addr, kernel_start, kernel_end, memmap_start, memmap_end, bootinfo_page))
        continue;

      if (g_free_top < MAX_FREE_PAGES) {
        g_free_pages[g_free_top++] = addr;
      }
    }
  }
}

void* pmm_alloc_page(void)
{
  if (g_free_top == 0) return (void*)0;
  uint64_t addr = g_free_pages[--g_free_top];
  return (void*)(uintptr_t)addr; // identity-mapped for now
}

void pmm_free_page(void *p)
{
  if (!p) return;
  uint64_t addr = (uint64_t)(uintptr_t)p;
  addr &= ~(PAGE_SIZE - 1);

  if (g_free_top < MAX_FREE_PAGES) {
    g_free_pages[g_free_top++] = addr;
  }
}

uint64_t pmm_free_count(void)
{
  return g_free_top;
}