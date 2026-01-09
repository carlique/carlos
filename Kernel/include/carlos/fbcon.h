#pragma once
#include <stdint.h>
#include <stddef.h>

void fbcon_init(uint64_t fb_base, uint64_t fb_size,
                uint32_t w, uint32_t h, uint32_t ppsl, uint32_t fmt);

void fbcon_clear(void);
void fbcon_putc(char c);
void fbcon_puts(const char *s);

void fbcon_enable_cursor(int enable);