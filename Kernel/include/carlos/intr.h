// include/carlos/intr.h
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef void (*irq_handler_t)(int irq, void *ctx);

void intr_init(void);              // installs IRQ gates (0x20..0x2F)

void intr_enable(void);
void intr_disable(void);

uint64_t intr_save(void);          // returns rflags
void     intr_restore(uint64_t f); // restores rflags (incl IF)

int irq_register(int irq, irq_handler_t fn, void *ctx);