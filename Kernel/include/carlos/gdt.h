#pragma once
#include <stdint.h>

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define KERNEL_TSS 0x18

void gdt_init(void);

// IST indices are 1..7 in the IDT gate
#define IST_DF 1