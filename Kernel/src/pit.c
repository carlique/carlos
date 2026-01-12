// src/pit.c
#include <stdint.h>
#include <carlos/pit.h>

// you likely already have these somewhere; if not:
static inline void outb(uint16_t port, uint8_t val){
  __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

void pit_init_hz(uint32_t hz){
  if (hz == 0) hz = 1000;

  // PIT input clock
  const uint32_t PIT_HZ = 1193182u;

  uint32_t div32 = PIT_HZ / hz;
  if (div32 < 1) div32 = 1;
  if (div32 > 65535) div32 = 65535;
  uint16_t div = (uint16_t)div32;

  // ch0, lobyte/hibyte, mode 2 (rate generator), binary
  outb(0x43, 0x34);
  outb(0x40, (uint8_t)(div & 0xFF));
  outb(0x40, (uint8_t)((div >> 8) & 0xFF));
}