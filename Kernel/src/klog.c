#include <stdint.h>
#include <stdarg.h>
#include <carlos/klog.h>
#include <carlos/uart.h>
#include <carlos/fbcon.h>
#include <carlos/bootinfo.h>

static int fb_enabled = 0;

void klog_enable_fb(const BootInfo *bi) {
  if (!bi || bi->fb_base == 0) return;
  fbcon_init(bi->fb_base, bi->fb_size, bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format);
  fb_enabled = 1;
}

static void sink_putc(char c) {
  uart_putc(c);
  if (fb_enabled) fbcon_putc(c);
}

static void sink_puts(const char *s) {
  for (; s && *s; s++) {
    if (*s == '\n') sink_putc('\r'); // keeps UART happy
    sink_putc(*s);
  }
}

static void put_hex_u64(uint64_t v, int prefix0x, int width, int zero_pad){
  static const char *hex = "0123456789ABCDEF";
  if (prefix0x) sink_puts("0x");

  // Collect digits reversed
  char tmp[16];
  int n = 0;

  if (v == 0) {
    tmp[n++] = '0';
  } else {
    while (v && n < 16) {
      tmp[n++] = hex[v & 0xF];
      v >>= 4;
    }
  }

  // Pad to width (minimum digits)
  while (n < width && n < 16) {
    tmp[n++] = zero_pad ? '0' : ' ';
  }

  // Emit forward
  for (int i = n - 1; i >= 0; i--) sink_putc(tmp[i]);
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

      case 'p': {
        void *v = va_arg(ap, void*);
        put_hex_u64((uint64_t)(uintptr_t)v, 1, 16, 1); // fixed 16 digits, zero padded
      } break;

      case 'x': {
        uint64_t v = longlong ? (uint64_t)va_arg(ap, unsigned long long)
                              : (uint64_t)va_arg(ap, unsigned int);

        // default width: 1 digit if no width specified
        int w = (width > 0) ? width : 1;
        put_hex_u64(v, 0, w, zero_pad);
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