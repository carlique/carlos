#include <stdint.h>
#include <carlos/idt.h>
#include <carlos/gdt.h>

typedef struct __attribute__((packed)) {
  uint16_t off0;
  uint16_t sel;
  uint8_t  ist;
  uint8_t  type_attr;
  uint16_t off1;
  uint32_t off2;
  uint32_t zero;
} IdtGate;

typedef struct __attribute__((packed)) {
  uint16_t limit;
  uint64_t base;
} IdtPtr;

static IdtGate g_idt[256];

extern void isr_stub_6(void);
extern void isr_stub_8(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);

static void set_gate(int vec, void *fn){
  uint64_t a = (uint64_t)fn;
  g_idt[vec].off0 = (uint16_t)(a & 0xFFFF);
  g_idt[vec].sel  = KERNEL_CS;     // now stable
  g_idt[vec].ist  = 0;
  if (vec == 8) g_idt[vec].ist = IST_DF; // double fault uses IST1
  g_idt[vec].type_attr = 0x8E;
  g_idt[vec].off1 = (uint16_t)((a >> 16) & 0xFFFF);
  g_idt[vec].off2 = (uint32_t)((a >> 32) & 0xFFFFFFFF);
  g_idt[vec].zero = 0;
}

static inline void lidt(const IdtPtr *p){
  __asm__ volatile ("lidt (%0)" :: "r"(p) : "memory");
}

void idt_init(void){
  for (int i=0;i<256;i++) {
    // leave empty (0) for now
    g_idt[i] = (IdtGate){0};
  }

  set_gate(6,  isr_stub_6);
  set_gate(8,  isr_stub_8);
  set_gate(13, isr_stub_13);
  set_gate(14, isr_stub_14);

  IdtPtr p = {
    .limit = (uint16_t)(sizeof(g_idt)-1),
    .base  = (uint64_t)g_idt
  };
  lidt(&p);
}