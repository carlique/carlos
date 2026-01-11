#include <stdint.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/phys.h>
#include <carlos/pmm.h>

// ---- Optional debug (off by default) ----
#define PMM_DEBUG 1

#if PMM_DEBUG
  #include <carlos/klog.h>
  #define PMM_LOG(...) kprintf(__VA_ARGS__)
#else
  #define PMM_LOG(...) ((void)0)
#endif

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

#define PAGE_SIZE 4096ULL
#define EfiConventionalMemory 7
#define PMM_MIN_ALLOC_PHYS (16ULL * 1024ULL * 1024ULL)   // 16 MiB

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
  if (in_range(page_phys, kernel_lo, kernel_hi)) return 1;
  if (in_range(page_phys, memmap_lo, memmap_hi)) return 1;
  if (in_range(page_phys, bi_lo, bi_hi)) return 1;
  return 0;
}

/* ---------- sort (heapsort, no libc) ---------- */
static void sift_down(uint64_t *a, uint64_t start, uint64_t end){
  uint64_t root = start;
  while (1){
    uint64_t child = root * 2 + 1;
    if (child > end) break;
    uint64_t swap = root;
    if (a[swap] < a[child]) swap = child;
    if (child + 1 <= end && a[swap] < a[child + 1]) swap = child + 1;
    if (swap == root) return;
    uint64_t tmp = a[root]; a[root] = a[swap]; a[swap] = tmp;
    root = swap;
  }
}

static void heap_sort_u64(uint64_t *a, uint64_t n){
  if (n < 2) return;

  // heapify (max-heap)
  for (uint64_t start = (n - 2) / 2 + 1; start > 0; start--){
    sift_down(a, start - 1, n - 1);
  }

  // sort (ascending)
  for (uint64_t end = n - 1; end > 0; end--){
    uint64_t tmp = a[end]; a[end] = a[0]; a[0] = tmp;
    sift_down(a, 0, end - 1);
  }
}

/* insert page_phys into sorted array */
static void insert_sorted(uint64_t page_phys){
  if (g_free_top >= MAX_FREE_PAGES) return;

  uint64_t lo = 0, hi = g_free_top;
  while (lo < hi){
    uint64_t mid = lo + ((hi - lo) / 2);
    if (g_free_pages[mid] < page_phys) lo = mid + 1;
    else hi = mid;
  }

  // shift right
  for (uint64_t i = g_free_top; i > lo; i--){
    g_free_pages[i] = g_free_pages[i - 1];
  }
  g_free_pages[lo] = page_phys;
  g_free_top++;
}

void pmm_init(const BootInfo *bi)
{
  g_free_top = 0;
  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) return;

  const uint64_t mm_base = bi->memmap;
  const uint64_t mm_size = bi->memmap_size;
  const uint64_t desc_sz = bi->memdesc_size;

  if (!mm_base || !mm_size || !desc_sz) return;

  uint64_t kernel_lo = align_down((uint64_t)(uintptr_t)&__kernel_start);
  uint64_t kernel_hi = align_up  ((uint64_t)(uintptr_t)&__kernel_end);

  uint64_t memmap_lo = align_down(mm_base);
  uint64_t memmap_hi = align_up(mm_base + mm_size);

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

      // HARD RESERVE low memory (UEFI often leaves important stuff there even if "Conventional")
      if (page_phys < PMM_MIN_ALLOC_PHYS)
        continue;

      if (is_reserved_page(page_phys, kernel_lo, kernel_hi, memmap_lo, memmap_hi, bi_lo, bi_hi))
        continue;

      if (g_free_top < MAX_FREE_PAGES) {
        g_free_pages[g_free_top++] = page_phys;
      }
    }
  }

  // Keep free list sorted so contiguous allocation is reliable
  heap_sort_u64(g_free_pages, g_free_top);
}

uint64_t pmm_alloc_page_phys(void)
{
  if (g_free_top == 0) return 0;
  // sorted ascending => take highest for O(1)
  return g_free_pages[--g_free_top];
}

void pmm_free_page_phys(uint64_t phys)
{
  if (!phys) return;
  phys &= ~(PAGE_SIZE - 1);
  insert_sorted(phys);
}

uint64_t pmm_free_count(void)
{
  return g_free_top;
}

uint64_t pmm_alloc_contig_pages_phys(uint64_t pages)
{
  if (pages == 0) return 0;
  if (pages == 1) return pmm_alloc_page_phys();
  
  if (g_free_top < pages) return 0;

  PMM_LOG("pmm: contig request pages=%llu free=%llu\n",
          (unsigned long long)pages, (unsigned long long)g_free_top);

  for (uint64_t i = 0; i + pages <= g_free_top; i++){
    uint64_t base = g_free_pages[i];
    int ok = 1;
    for (uint64_t j = 1; j < pages; j++){
      if (g_free_pages[i + j] != base + j * PAGE_SIZE) { ok = 0; break; }
    }
    if (!ok) continue;

    for (uint64_t k = i; k + pages < g_free_top; k++){
      g_free_pages[k] = g_free_pages[k + pages];
    }
    g_free_top -= pages;

    PMM_LOG("pmm: contig ok base=0x%llx free=%llu\n",
            (unsigned long long)base, (unsigned long long)g_free_top);
    return base;
  }

  PMM_LOG("pmm: contig FAIL pages=%llu free=%llu\n",
          (unsigned long long)pages, (unsigned long long)g_free_top);
  return 0;
}

void pmm_free_contig_pages_phys(uint64_t base_phys, uint64_t pages)
{
  if (!base_phys || pages == 0) return;
  base_phys &= ~(PAGE_SIZE - 1);

  for (uint64_t i = 0; i < pages; i++){
    pmm_free_page_phys(base_phys + i * PAGE_SIZE);
  }
}