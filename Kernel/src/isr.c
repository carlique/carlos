#include <stdint.h>
#include <carlos/isr.h>
#include <carlos/klog.h>

static volatile int g_panic = 0;

static inline uint64_t rd_cr2(void){
  uint64_t v;
  __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
  return v;
}

static void halt_forever(void){
  __asm__ volatile ("cli");
  for (;;) __asm__ volatile ("hlt");
}

void isr_common_handler(IsrFrame *f)
{
  if (__atomic_exchange_n(&g_panic, 1, __ATOMIC_SEQ_CST)) {
    // if we re-enter, just return (don’t deadlock the system)
    return;
  }

  __asm__ volatile ("cli");

  kprintf("\n=== EXC %llu ===\n", f->vector);
  kprintf("err=0x%llx rip=0x%llx cs=0x%llx rflags=0x%llx\n",
          f->error, f->rip, f->cs, f->rflags);

  if (f->vector == 14) {
    uint64_t cr2 = rd_cr2();
    kprintf("cr2=0x%llx\n", cr2);
  }

   // For TESTING: allow returning from “safe-ish” traps
  //if (f->vector == 3 || f->vector == 6) {
  //  kprintf("ISR: returning from vector %llu\n", f->vector);
  //  __asm__ volatile ("sti");
  //  return;
  //}

  kprintf("HALT.\n");
  halt_forever();
}