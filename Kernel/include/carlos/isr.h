#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) IsrFrame {
  uint64_t vector;   // pushed by our stub
  uint64_t error;    // pushed by CPU for some vectors; we push 0 for others
  uint64_t rip;      // pushed by CPU
  uint64_t cs;       // pushed by CPU
  uint64_t rflags;   // pushed by CPU
} IsrFrame;

void isr_common_handler(IsrFrame *f);