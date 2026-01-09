#include <stdint.h>
#include <stddef.h>
#include <carlos/gdt.h>

// 64-bit TSS (AMD64)
typedef struct __attribute__((packed)) {
  uint32_t rsv0;
  uint64_t rsp0;
  uint64_t rsp1;
  uint64_t rsp2;
  uint64_t rsv1;
  uint64_t ist1;
  uint64_t ist2;
  uint64_t ist3;
  uint64_t ist4;
  uint64_t ist5;
  uint64_t ist6;
  uint64_t ist7;
  uint64_t rsv2;
  uint16_t rsv3;
  uint16_t iomap_base;
} Tss64;

typedef struct __attribute__((packed)) {
  uint16_t limit;
  uint64_t base;
} Gdtr;

// GDT: null, kcode, kdata, tss (16 bytes = 2 entries)
static uint64_t g_gdt[5] __attribute__((aligned(16)));
static Tss64    g_tss;

static uint8_t  g_df_stack[16 * 1024] __attribute__((aligned(16))); // IST1 for #DF
static uint8_t  g_kstack[16 * 1024]   __attribute__((aligned(16))); // "rsp0" placeholder

static inline void lgdt(const Gdtr *gdtr){
  __asm__ volatile ("lgdt (%0)" :: "r"(gdtr) : "memory");
}

static inline void ltr(uint16_t sel){
  __asm__ volatile ("ltr %0" :: "r"(sel) : "memory");
}

static inline void reload_segments(void){
  // Load DS/ES/SS with KERNEL_DS, and reload CS via far return.
  __asm__ volatile (
    "movw %[ds], %%ax \n"
    "movw %%ax, %%ds  \n"
    "movw %%ax, %%es  \n"
    "movw %%ax, %%ss  \n"
    // far reload CS using lretq
    "pushq %[cs]      \n"
    "leaq 1f(%%rip), %%rax \n"
    "pushq %%rax      \n"
    "lretq            \n"
    "1:               \n"
    :
    : [cs] "i"(KERNEL_CS), [ds] "i"(KERNEL_DS)
    : "rax", "memory"
  );
}

static uint64_t gdt_code64(void){
  // 64-bit code: base=0, limit=0, type=0x9A, gran=1, L=1
  // 0x00AF9A000000FFFF is a common “flat” long-mode code descriptor.
  return 0x00AF9A000000FFFFULL;
}

static uint64_t gdt_data(void){
  // data: base=0, limit=0xFFFFF, type=0x92, gran=1
  return 0x00CF92000000FFFFULL;
}

static void gdt_set_tss(uint64_t base, uint32_t limit){
  // TSS descriptor is 16 bytes (2 GDT entries).
  // Low 8 bytes:
  uint64_t lo =
    ((uint64_t)(limit & 0xFFFF)) |
    ((uint64_t)(base  & 0xFFFFFF) << 16) |
    ((uint64_t)0x89 << 40) |                 // type=0x9 (available 64-bit TSS), present=1
    ((uint64_t)((limit >> 16) & 0xF) << 48) |
    ((uint64_t)((base  >> 24) & 0xFF) << 56);

  // High 8 bytes: upper 32 bits of base in low dword
  uint64_t hi = (base >> 32) & 0xFFFFFFFFULL;

  g_gdt[3] = lo;
  g_gdt[4] = hi;
}

void gdt_init(void){
  // Zero TSS
  for (size_t i = 0; i < sizeof(g_tss); i++) ((uint8_t*)&g_tss)[i] = 0;

  // Set stacks
  uint64_t df_top = (uint64_t)(g_df_stack + sizeof(g_df_stack));
  uint64_t k_top  = (uint64_t)(g_kstack  + sizeof(g_kstack));

  g_tss.rsp0 = k_top;     // for future ring3->ring0 transitions
  g_tss.ist1 = df_top;    // IST1 reserved for double fault
  g_tss.iomap_base = (uint16_t)sizeof(Tss64); // no IO bitmap

  // Build GDT
  g_gdt[0] = 0;
  g_gdt[1] = gdt_code64();
  g_gdt[2] = gdt_data();
  gdt_set_tss((uint64_t)(uintptr_t)&g_tss, (uint32_t)(sizeof(Tss64) - 1));

  Gdtr gdtr = {
    .limit = (uint16_t)(sizeof(g_gdt) - 1),
    .base  = (uint64_t)(uintptr_t)g_gdt
  };

  lgdt(&gdtr);
  reload_segments();
  ltr(KERNEL_TSS);
}