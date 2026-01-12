// src/time.c
#include <stdint.h>
#include <carlos/time.h>
#include <carlos/hpet.h>
#include <carlos/intr.h>
#include <carlos/pit.h>

static int g_time_ok = 0;

volatile uint64_t g_ticks_ms = 0;

// called from IRQ0 handler (adapter below)
void timer_tick_irq0(void){
  g_ticks_ms++;
}

static inline void cpu_relax_hlt(void){
  __asm__ volatile ("sti; hlt" ::: "memory");
}

static void irq0_timer(int irq, void *ctx){
  (void)irq;
  (void)ctx;
  timer_tick_irq0();
}

int time_init(void){
  if (hpet_init_from_acpi() == 0) {
    g_time_ok = 1;
    return 0;
  }
  g_time_ok = 0;
  return -1;
}

void time_timer_start(void){
  // IRQ0 = PIT
  irq_register(0, irq0_timer, 0);

  // 1000Hz => ~1ms tick increments g_ticks_ms in IRQ0
  pit_init_hz(1000);
}

uint64_t time_now_ns(void){
  if (!g_time_ok) return 0;
  return hpet_now_ns();
}

void time_sleep_ms(uint64_t ms){
  uint64_t deadline = g_ticks_ms + (uint64_t)ms;
  while (g_ticks_ms < deadline){
    cpu_relax_hlt();
  }
}