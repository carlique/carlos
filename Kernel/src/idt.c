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

// Declare stubs 0..31 using your naming scheme
#define DECL_ISR(n) extern void isr##n(void);
DECL_ISR(0)  DECL_ISR(1)  DECL_ISR(2)  DECL_ISR(3)
DECL_ISR(4)  DECL_ISR(5)  DECL_ISR(6)  DECL_ISR(7)
DECL_ISR(8)  DECL_ISR(9)  DECL_ISR(10) DECL_ISR(11)
DECL_ISR(12) DECL_ISR(13) DECL_ISR(14) DECL_ISR(15)
DECL_ISR(16) DECL_ISR(17) DECL_ISR(18) DECL_ISR(19)
DECL_ISR(20) DECL_ISR(21) DECL_ISR(22) DECL_ISR(23)
DECL_ISR(24) DECL_ISR(25) DECL_ISR(26) DECL_ISR(27)
DECL_ISR(28) DECL_ISR(29) DECL_ISR(30) DECL_ISR(31)
#undef DECL_ISR

static void *const g_isr[32] = {
  isr0,  isr1,  isr2,  isr3,
  isr4,  isr5,  isr6,  isr7,
  isr8,  isr9,  isr10, isr11,
  isr12, isr13, isr14, isr15,
  isr16, isr17, isr18, isr19,
  isr20, isr21, isr22, isr23,
  isr24, isr25, isr26, isr27,
  isr28, isr29, isr30, isr31,
};

static void set_gate(int vec, void *fn){
  uint64_t a = (uint64_t)fn;
  g_idt[vec].off0 = (uint16_t)(a & 0xFFFF);
  g_idt[vec].sel  = KERNEL_CS;
  g_idt[vec].ist  = 0;
  if (vec == 8) g_idt[vec].ist = IST_DF;   // keep your DF IST
  g_idt[vec].type_attr = 0x8E;             // interrupt gate
  g_idt[vec].off1 = (uint16_t)((a >> 16) & 0xFFFF);
  g_idt[vec].off2 = (uint32_t)((a >> 32) & 0xFFFFFFFF);
  g_idt[vec].zero = 0;
}

static inline void lidt(const IdtPtr *p){
  __asm__ volatile ("lidt (%0)" :: "r"(p) : "memory");
}

void idt_init(void){
  for (int i=0;i<256;i++) g_idt[i] = (IdtGate){0};

  // Install all CPU exceptions (0..31)
  for (int v = 0; v < 32; v++) {
    set_gate(v, g_isr[v]);
  }

  IdtPtr p = {
    .limit = (uint16_t)(sizeof(g_idt)-1),
    .base  = (uint64_t)g_idt
  };
  lidt(&p);
}