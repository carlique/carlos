#pragma once
#include <stdint.h>

void uart_init(void);
int  uart_try_getc(char *out);   // returns 1 if a char was read, else 0
void uart_putc(char c);
void uart_puts(const char *s);