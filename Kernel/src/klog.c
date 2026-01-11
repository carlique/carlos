#include <stdint.h>
#include <stdarg.h>
#include <carlos/klog.h>
#include <carlos/uart.h>
#include <carlos/fbcon.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/phys.h>

static int fb_enabled = 0;

#ifndef KLOG_DEFAULT_LEVEL
  #define KLOG_DEFAULT_LEVEL KLOG_INFO
#endif

#ifndef KLOG_DEFAULT_MASK
  #define KLOG_DEFAULT_MASK KLOG_MOD_ALL
#endif

volatile uint8_t  g_klog_level = (uint8_t)KLOG_DEFAULT_LEVEL;
volatile uint32_t g_klog_mask  = (uint32_t)KLOG_DEFAULT_MASK;

#define KLOG_RING 1
#define KLOG_RING_SIZE 8192

#if KLOG_RING
static char g_klog_ring[KLOG_RING_SIZE];
static uint32_t g_klog_ring_head = 0;
static uint8_t  g_klog_ring_full = 0;

static void ring_putc(char c) {
  g_klog_ring[g_klog_ring_head++] = c;
  if (g_klog_ring_head >= KLOG_RING_SIZE) {
    g_klog_ring_head = 0;
    g_klog_ring_full = 1;
  }
}

// dump ring via kputc 
// TODO: later wire to shell command "dmesg"
void klog_ring_dump(void) {
  if (!g_klog_ring_full) {
    for (uint32_t i = 0; i < g_klog_ring_head; i++) kputc(g_klog_ring[i]);
    return;
  }
  for (uint32_t i = g_klog_ring_head; i < KLOG_RING_SIZE; i++) kputc(g_klog_ring[i]);
  for (uint32_t i = 0; i < g_klog_ring_head; i++) kputc(g_klog_ring[i]);
}
#endif

__attribute__((noreturn))
void kpanic_impl(const char *file, int line, const char *msg) {
  kprintf("\nPANIC %s:%d: %s\n", file, line, msg ? msg : "(null)");
  for (;;) {
    __asm__ volatile ("cli; hlt");
  }
}

void klog_enable_fb(const BootInfo *bi) {
  if (!bi || bi->fb_base == 0) return;
  fbcon_init(bi->fb_base, bi->fb_size, 
            bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format);
  fb_enabled = 1;
}

static void sink_putc(char c) {
#if KLOG_RING
  ring_putc(c);
#endif
  uart_putc(c);
  if (fb_enabled) fbcon_putc(c);
}

static void sink_puts(const char *s) {
  for (; s && *s; s++) {
    if (*s == '\n') sink_putc('\r'); // keeps UART happy
    sink_putc(*s);
  }
}

static void put_hex_u64_fmt(uint64_t v, int prefix0x, int width, int pad_zero) {
  static const char *hex = "0123456789ABCDEF";
  char buf[16];
  int n = 0;

  // digits (LSB first)
  do {
    buf[n++] = hex[v & 0xF];
    v >>= 4;
  } while (v && n < (int)sizeof(buf));

  if (width < n) width = n;

  if (prefix0x) sink_puts("0x");

  char pad = pad_zero ? '0' : ' ';
  for (int i = n; i < width; i++) sink_putc(pad);
  for (int i = n - 1; i >= 0; i--) sink_putc(buf[i]);
}

static void put_dec_u64(uint64_t v) {
  char buf[21];
  int i = 0;
  if (v == 0) { sink_putc('0'); return; }
  while (v > 0 && i < (int)sizeof(buf)) {
    buf[i++] = '0' + (v % 10);
    v /= 10;
  }
  for (int j = i - 1; j >= 0; j--) sink_putc(buf[j]);
}

static void put_dec_i64(int64_t v) {
  if (v < 0) { sink_putc('-'); put_dec_u64((uint64_t)(-v)); }
  else put_dec_u64((uint64_t)v);
}

void klog_init(void) {
  uart_init();
}

void kputc(char c) {
  sink_putc(c);
}

void kputs(const char *s) {
  sink_puts(s);
}

void kvprintf(const char *fmt, va_list ap) {
  for (const char *p = fmt; *p; p++) {
    if (*p != '%') {
      sink_putc(*p);
      continue;
    }

    p++; // skip '%'
        // flags: only '0' supported for now
    int zero_pad = 0;
    if (*p == '0') { zero_pad = 1; p++; if (*p == 0) break; }

    // width: decimal digits
    int width = 0;
    while (*p >= '0' && *p <= '9') {
      width = width * 10 + (*p - '0');
      p++;
      if (*p == 0) break;
    }

    if (*p == 0) break;

    // Handle %%
    if (*p == '%') {
      sink_putc('%');
      continue;
    }

    // Very small length handling: support "ll" for 64-bit hex/dec
    int longlong = 0;
    if (*p == 'l' && *(p+1) == 'l') {
      longlong = 1;
      p += 2;
      if (*p == 0) break;
    }

    switch (*p) {
      case 's': {
        const char *s = va_arg(ap, const char*);
        sink_puts(s ? s : "(null)");
      } break;

      case 'c': {
        int c = va_arg(ap, int);
        sink_putc((char)c);
      } break;

      // keep your parsing of zero_pad + width + longlong
      case 'p': {
        void *v = va_arg(ap, void*);
        put_hex_u64_fmt((uint64_t)(uintptr_t)v, 1, 16, 1);   // 0x + 16 hex digits
      } break;

      case 'x': {
        uint64_t v = longlong ? (uint64_t)va_arg(ap, unsigned long long)
                              : (uint64_t)va_arg(ap, unsigned int);
        put_hex_u64_fmt(v, 0, width ? width : 1, zero_pad);  // supports %02x etc
      } break;

      case 'u': {
        uint64_t v = longlong ? va_arg(ap, unsigned long long) : (uint64_t)va_arg(ap, unsigned int);
        put_dec_u64(v);
      } break;

      case 'd': {
        int64_t v = longlong ? (int64_t)va_arg(ap, long long) : (int64_t)va_arg(ap, int);
        put_dec_i64(v);
      } break;

      default:
        // Unknown specifier: print it literally to make bugs obvious
        sink_putc('%');
        sink_putc(*p);
        break;
    }
  }
}

void kprintf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  kvprintf(fmt, ap);
  va_end(ap);
}

void klog_set_level(uint8_t level) { g_klog_level = level; }
void klog_set_mask (uint32_t mask) { g_klog_mask  = mask; }

const char* klog_level_name(uint8_t lvl){
  switch (lvl){
    case KLOG_ERR:   return "err";
    case KLOG_WARN:  return "warn";
    case KLOG_INFO:  return "info";
    case KLOG_DBG:   return "dbg";
    case KLOG_TRACE: return "trace";
    default:         return "?";
  }
}

static int is_dec_digit(char c){ return (c >= '0' && c <= '9'); }
static int hexval_local(char c){
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static int parse_u32_dec_local(const char *s, uint32_t *out){
  if (!s || !*s || !is_dec_digit(*s)) return -1;
  uint64_t v = 0;
  while (*s && is_dec_digit(*s)){
    v = v * 10 + (uint64_t)(*s - '0');
    if (v > 0xFFFFFFFFull) return -1;
    s++;
  }
  if (*s != 0) return -1; // reject trailing junk
  *out = (uint32_t)v;
  return 0;
}

static int parse_u32_hex_local(const char *s, uint32_t *out){
  if (!s || s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) return -1;
  s += 2;
  if (!*s) return -1;

  uint64_t v = 0;
  while (*s){
    int h = hexval_local(*s);
    if (h < 0) return -1;
    v = (v << 4) | (uint64_t)h;
    if (v > 0xFFFFFFFFull) return -1;
    s++;
  }
  *out = (uint32_t)v;
  return 0;
}

int klog_parse_level(const char *s, uint8_t *out_level){
  if (!s || !*s || !out_level) return -1;

  // numeric 0..255 (but you really only use 0..4)
  if (is_dec_digit(s[0])) {
    uint32_t v = 0;
    if (parse_u32_dec_local(s, &v) != 0) return -1;
    if (v > 255) return -1;
    *out_level = (uint8_t)v;
    return 0;
  }

  if (kstreq(s, "err")   || kstreq(s, "KLOG_ERR"))   { *out_level = KLOG_ERR;   return 0; }
  if (kstreq(s, "warn")  || kstreq(s, "KLOG_WARN"))  { *out_level = KLOG_WARN;  return 0; }
  if (kstreq(s, "info")  || kstreq(s, "KLOG_INFO"))  { *out_level = KLOG_INFO;  return 0; }
  if (kstreq(s, "dbg")   || kstreq(s, "KLOG_DBG"))   { *out_level = KLOG_DBG;   return 0; }
  if (kstreq(s, "trace") || kstreq(s, "KLOG_TRACE")) { *out_level = KLOG_TRACE; return 0; }

  return -1;
}

static int klog_parse_mask_name(const char *s, uint32_t *out){
  if (!s || !*s) return -1;

  if (kstreq(s, "all") || kstreq(s, "KLOG_MOD_ALL")) { *out = KLOG_MOD_ALL; return 0; }

  if (kstreq(s, "core")  || kstreq(s, "KLOG_MOD_CORE"))  { *out = KLOG_MOD_CORE; return 0; }
  if (kstreq(s, "pmm")   || kstreq(s, "KLOG_MOD_PMM"))   { *out = KLOG_MOD_PMM;  return 0; }
  if (kstreq(s, "kmem")  || kstreq(s, "KLOG_MOD_KMEM"))  { *out = KLOG_MOD_KMEM; return 0; }
  if (kstreq(s, "exec")  || kstreq(s, "KLOG_MOD_EXEC"))  { *out = KLOG_MOD_EXEC; return 0; }
  if (kstreq(s, "fat")   || kstreq(s, "KLOG_MOD_FAT"))   { *out = KLOG_MOD_FAT;  return 0; }
  if (kstreq(s, "fs")    || kstreq(s, "KLOG_MOD_FS"))    { *out = KLOG_MOD_FS;   return 0; }
  if (kstreq(s, "kapi")  || kstreq(s, "KLOG_MOD_KAPI"))  { *out = KLOG_MOD_KAPI; return 0; }
  if (kstreq(s, "shell") || kstreq(s, "KLOG_MOD_SHELL")) { *out = KLOG_MOD_SHELL;return 0; }

  return -1;
}

int klog_parse_mask(const char *s, uint32_t *out_mask){
  if (!s || !*s || !out_mask) return -1;

  // allow names
  uint32_t named = 0;
  if (klog_parse_mask_name(s, &named) == 0) { *out_mask = named; return 0; }

  // allow hex or decimal
  uint32_t v = 0;
  if (parse_u32_hex_local(s, &v) == 0) { *out_mask = v; return 0; }
  if (parse_u32_dec_local(s, &v) == 0) { *out_mask = v; return 0; }

  return -1;
}

int klog_set_level_str(const char *s){
  uint8_t lvl = 0;
  if (klog_parse_level(s, &lvl) != 0) return -1;
  klog_set_level(lvl);
  return 0;
}

int klog_set_mask_str(const char *s){
  uint32_t m = 0;
  if (klog_parse_mask(s, &m) != 0) return -1;
  klog_set_mask(m);
  return 0;
}

void klog_print_state(void){
  kprintf("log: level=%u (%s) mask=0x%08x\n",
          (unsigned)g_klog_level, klog_level_name(g_klog_level),
          (unsigned)g_klog_mask);
}

void klog_print_help(void){
  kputs("usage:\n");
  kputs("  log                      - show current\n");
  kputs("  log <lvl>                - set level (err|warn|info|dbg|trace|N)\n");
  kputs("  log <lvl> <mask>         - set level+mask (mask: 0x.. | dec | name)\n");
  kputs("mask names: all core pmm kmem exec fat fs kapi shell\n");
}