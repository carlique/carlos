#pragma once
#include <stdint.h>

typedef struct IrqCtrl {
  void (*init)(void);
  void (*mask)(int irq);
  void (*unmask)(int irq);
  void (*eoi)(int irq);
  int  (*vec_for_irq)(int irq);  // maps logical IRQ -> IDT vector
} IrqCtrl;

const IrqCtrl* irq_ctrl_pic(void);   // now
// const IrqCtrl* irq_ctrl_apic(void); // later