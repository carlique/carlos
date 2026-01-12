#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <carlos/boot/bootinfo.h>
#include <carlos/str.h>

typedef enum {
  KLOG_ERR  = 0,
  KLOG_WARN = 1,
  KLOG_INFO = 2,
  KLOG_DBG  = 3,
  KLOG_TRACE= 4
} KLogLevel;

#ifndef KLOG_DEFAULT_LEVEL
  #define KLOG_DEFAULT_LEVEL KLOG_INFO
#endif

#ifndef KLOG_DEFAULT_MASK
  #define KLOG_DEFAULT_MASK  KLOG_MOD_ALL
#endif

// runtime controls
extern volatile uint8_t  g_klog_level;
extern volatile uint32_t g_klog_mask;

// module bits (pick what you want, example)
enum {
  KLOG_MOD_CORE = 1u<<0,
  KLOG_MOD_PMM  = 1u<<1,
  KLOG_MOD_KMEM = 1u<<2,
  KLOG_MOD_EXEC = 1u<<3,
  KLOG_MOD_FAT  = 1u<<4,
  KLOG_MOD_FS   = 1u<<5,
  KLOG_MOD_KAPI = 1u<<6,
  KLOG_MOD_SHELL= 1u<<7,
  KLOG_MOD_DISK = 1u<<8,
  KLOG_MOD_AHCI = 1u<<9,
  KLOG_MOD_INTR = 1u<<10,
  KLOG_MOD_PIC  = 1u<<11,
  KLOG_MOD_ALL  = 0xFFFFFFFFu
};

#define KLOG(mod, lvl, ...) \
  do { \
    if ((uint8_t)(lvl) <= g_klog_level && (g_klog_mask & (uint32_t)(mod))) { \
      kprintf(__VA_ARGS__); \
    } \
  } while(0)

// panic/assert
__attribute__((noreturn))
void kpanic_impl(const char *file, int line, const char *msg);

#define KPANIC(msg) kpanic_impl(__FILE__, __LINE__, (msg))
#define KASSERT(x) do { if (!(x)) kpanic_impl(__FILE__, __LINE__, "assert: " #x); } while(0)

void klog_enable_fb(const BootInfo *bi);

void klog_init(void);
void kputc(char c);
void kputs(const char *s);

// printf-like logger to UART
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);

// parse helpers (return 0 on success, <0 on error)
int klog_parse_level(const char *s, uint8_t *out_level);
int klog_parse_mask (const char *s, uint32_t *out_mask);

// setters that also accept strings
int klog_set_level_str(const char *s);       // "dbg" / "3" / "KLOG_DBG"
int klog_set_mask_str (const char *s);       // "0x2" / "2" / "KLOG_MOD_PMM"

// direct setters
void klog_set_level(uint8_t level);
void klog_set_mask(uint32_t mask);

// pretty-printers (optional but handy for shell)
const char* klog_level_name(uint8_t lvl);
void klog_print_state(void);   // prints current level/mask
void klog_print_help(void);    // prints usage + module bits