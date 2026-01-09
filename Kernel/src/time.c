#include <stdint.h>
#include <carlos/time.h>
#include <carlos/hpet.h>

static int g_time_ok = 0;

int time_init(void){
  if (hpet_init_from_acpi() == 0) {
    g_time_ok = 1;
    return 0;
  }
  g_time_ok = 0;
  return -1;
}

uint64_t time_now_ns(void){
  if (!g_time_ok) return 0;
  return hpet_now_ns();
}

void time_sleep_ms(uint64_t ms){
  if (!g_time_ok) return;

  uint64_t start = time_now_ns();
  uint64_t goal  = start + ms * 1000000ULL;

  while (time_now_ns() < goal) {
    __asm__ volatile ("pause");
  }
}