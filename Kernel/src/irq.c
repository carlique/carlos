// src/irq.c
#include <carlos/isr.h>
#include <carlos/pic.h>
#include <carlos/intr.h>

// from intr.c
void intr_dispatch_irq(int irq);

#define IRQ_BASE_VEC 0x20

void irq_common_handler(IsrFrame *f){
  // IRQ vector -> irq index
  int vec = (int)f->vector;
  int irq = vec - IRQ_BASE_VEC;

  // dispatch handler (if any)
  intr_dispatch_irq(irq);

  // EOI (PIC for now)
  if (irq >= 0 && irq < 16) pic_eoi((uint8_t)irq);
}