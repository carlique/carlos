// src/intr.c
#include <carlos/intr.h>
#include <carlos/idt.h>

#define IRQ_BASE_VEC 0x20
#define IRQ_COUNT    16

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static void *const g_irq_stubs[IRQ_COUNT] = {
  irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
  irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

typedef struct {
  irq_handler_t fn;
  void *ctx;
} IrqSlot;

static IrqSlot g_irq[IRQ_COUNT];

void intr_disable(void){
  __asm__ volatile ("cli" ::: "memory");
}

void intr_enable(void){
  __asm__ volatile ("sti" ::: "memory");
}

uint64_t intr_save(void){
  uint64_t f;
  __asm__ volatile ("pushfq; popq %0" : "=r"(f) :: "memory");
  return f;
}

void intr_restore(uint64_t f){
  __asm__ volatile ("pushq %0; popfq" :: "r"(f) : "memory", "cc");
}

int irq_register(int irq, irq_handler_t fn, void *ctx){
  if (irq < 0 || irq >= IRQ_COUNT) return -1;
  g_irq[irq].fn  = fn;
  g_irq[irq].ctx = ctx;
  return 0;
}

// Called from irq.c
void intr_dispatch_irq(int irq){
  if (irq < 0 || irq >= IRQ_COUNT) return;
  irq_handler_t fn = g_irq[irq].fn;
  void *ctx = g_irq[irq].ctx;
  if (fn) fn(irq, ctx);
}

void intr_init(void){
  // install 0x20..0x2F gates
  for (int i = 0; i < IRQ_COUNT; i++){
    idt_set_gate(IRQ_BASE_VEC + i, g_irq_stubs[i], IDT_TYPE_INTGATE, 0);
    g_irq[i] = (IrqSlot){0};
  }
}