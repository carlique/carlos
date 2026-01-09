#include <carlos/isr.h>
#include <carlos/klog.h>

void isr_common_handler(IsrFrame *f){
  kprintf("\n=== EXCEPTION ===\n");
  kprintf("vec=%llu err=%llu\n", f->vector, f->error);
  kprintf("rip=%p cs=%p rflags=%p\n",
          (void*)f->rip, (void*)f->cs, (void*)f->rflags);

  if (f->vector == 14) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    kprintf("pagefault cr2=%p\n", (void*)cr2);
  }

  if (f->vector == 14) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

    kprintf("pagefault cr2=%p err=", (void*)cr2);
    kprintf("%llu\n", f->error);

    // Optional: decode common bits
    // bit0 P: 0=not-present,1=protection
    // bit1 W/R: 1=write
    // bit2 U/S: 1=user
    // bit3 RSVD
    // bit4 I/D instruction fetch
  }
}