#pragma once
#include <stdint.h>

#define IDT_TYPE_INTGATE 0x8E   // P=1, DPL=0, Type=0xE
#define IDT_TYPE_TRAPGATE 0x8F  // P=1, DPL=0, Type=0xF

void idt_init(void);

// Install/replace one IDT gate.
// - type_attr typically IDT_TYPE_INTGATE or IDT_TYPE_TRAPGATE
// - ist = 0 for default, or IST index (1..7)
void idt_set_gate(int vec, void *fn, uint8_t type_attr, uint8_t ist);

// Optional explicit reload (idt_init already loads once)
void idt_load(void);