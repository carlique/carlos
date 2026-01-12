// include/carlos/pic.h
#pragma once
#include <stdint.h>

void pic_init(uint8_t off_master, uint8_t off_slave);
void pic_set_mask(uint8_t irq, int masked);
void pic_eoi(uint8_t irq);