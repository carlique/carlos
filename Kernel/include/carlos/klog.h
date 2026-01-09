#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <carlos/bootinfo.h>

void klog_enable_fb(const BootInfo *bi);

void klog_init(void);
void kputc(char c);
void kputs(const char *s);

// printf-like logger to UART
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);