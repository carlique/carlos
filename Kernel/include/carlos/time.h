#pragma once
#include <stdint.h>

extern volatile uint64_t g_ticks_ms;

int  time_init(void);         // returns 0 on success
void time_timer_start(void);  // start periodic timer IRQs

uint64_t time_now_ns(void);   // monotonic ns since time_init
void time_sleep_ms(uint64_t ms);