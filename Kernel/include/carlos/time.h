#pragma once
#include <stdint.h>

int  time_init(void);         // returns 0 on success
uint64_t time_now_ns(void);   // monotonic ns since time_init
void time_sleep_ms(uint64_t ms);