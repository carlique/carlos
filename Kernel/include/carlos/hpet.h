#pragma once
#include <stdint.h>

int  hpet_init_from_acpi(void);  // returns 0 on success
uint64_t hpet_now_ns(void);