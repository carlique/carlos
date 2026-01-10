#include <stdint.h>
#include <stddef.h>
#include <carlos/fbcon.h>
#include <carlos/phys.h>
#include "font8x8_basic.h"

#define CHAR_W 8
#define CHAR_H 16

// EFI_GRAPHICS_PIXEL_FORMAT values (UEFI spec)
#define PixelRedGreenBlueReserved8BitPerColor 0
#define PixelBlueGreenRedReserved8BitPerColor 1

static volatile uint32_t *fb = 0;
static uint64_t fb_phys = 0;   // optional, but nice for debugging
static uint32_t fb_w=0, fb_h=0, fb_ppsl=0, fb_fmt=0;
static uint32_t cur_x=0, cur_y=0;

static int cursor_enabled = 1;
static int cursor_visible = 0;

static uint32_t fg = 0x00FFFFFF; // white
static uint32_t bg = 0x00000000; // black

// Pack RGB values into framebuffer pixel format
static inline uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
  // framebuffer is 32bpp, but channel order depends on PixelFormat
  if (fb_fmt == PixelBlueGreenRedReserved8BitPerColor) {
    // BGRA
    return ((uint32_t)b) | ((uint32_t)g<<8) | ((uint32_t)r<<16);
  }
  // default to RGBA
  return ((uint32_t)r) | ((uint32_t)g<<8) | ((uint32_t)b<<16);
}

static inline void put_px(uint32_t x, uint32_t y, uint32_t c) {
  fb[y * fb_ppsl + x] = c;
}

// Scroll framebuffer console up by one character row
static void scroll(void) {
  // scroll up by CHAR_H pixels
  uint32_t rows_px = fb_h;
  uint32_t move_px = CHAR_H;
  uint32_t pitch = fb_ppsl;

  for (uint32_t y = 0; y < rows_px - move_px; y++) {
    for (uint32_t x = 0; x < fb_w; x++) {
      fb[y*pitch + x] = fb[(y+move_px)*pitch + x];
    }
  }
  // clear bottom area
  for (uint32_t y = rows_px - move_px; y < rows_px; y++) {
    for (uint32_t x = 0; x < fb_w; x++) {
      fb[y*pitch + x] = bg;
    }
  }
  if (cur_y > 0) cur_y--;
}

/* 
    Draw character 'ch' at character cell (cx, cy)
*/
static void draw_char(uint32_t cx, uint32_t cy, char ch) {
  if (ch < 32 || ch > 127) ch = '?';
  const uint8_t *g = (const uint8_t*)font8x8_basic[(unsigned char)ch];

  uint32_t px0 = cx * CHAR_W;
  uint32_t py0 = cy * CHAR_H;

  for (uint32_t row = 0; row < 8; row++) {
    uint8_t bits = g[row];
    for (uint32_t dy = 0; dy < 2; dy++) {
        for (uint32_t col = 0; col < 8; col++) {
        uint32_t c = (bits & (1u << col)) ? fg : bg;
        put_px(px0 + col, py0 + row*2 + dy, c);
    }
  }
}
}

/* 
    Initialize framebuffer console
    param fb_base Physical address of framebuffer
    param fb_size Size of framebuffer in bytes
    param w Width in pixels
    param h Height in pixels
    param ppsl Pixels per scanline
    param fmt Pixel format (EFI_GRAPHICS_PIXEL_FORMAT)
*/
void fbcon_init(uint64_t fb_base_phys, uint64_t fb_size,
                uint32_t w, uint32_t h, uint32_t ppsl, uint32_t fmt)
{
  (void)fb_size;

  fb_phys = fb_base_phys;
  if (fb_phys == 0 || w == 0 || h == 0 || ppsl == 0) {
    fb = 0;
    fb_w = fb_h = fb_ppsl = fb_fmt = 0;
    return;
  }

  fb = (volatile uint32_t*)phys_to_ptr(fb_phys);
  fb_w = w; fb_h = h; fb_ppsl = ppsl; fb_fmt = fmt;

  fg = pack_rgb(255,255,255);
  bg = pack_rgb(0,0,0);
  cur_x = 0; cur_y = 0;

  fbcon_clear();
}

// Invert character cell at (cx, cy)
static void invert_cell(uint32_t cx, uint32_t cy) {
  if (!fb) return;

  uint32_t px0 = cx * CHAR_W;
  uint32_t py0 = cy * CHAR_H;

  for (uint32_t y = 0; y < CHAR_H; y++) {
    for (uint32_t x = 0; x < CHAR_W; x++) {
      uint32_t px = px0 + x;
      uint32_t py = py0 + y;
      if (px < fb_w && py < fb_h) {
        uint32_t i = py * fb_ppsl + px;
        fb[i] ^= 0x00FFFFFF; // invert RGB bits (good enough for our 32bpp formats)
      }
    }
  }
}

// Show cursor
static void cursor_show(void) {
  if (!cursor_enabled || cursor_visible) return;
  invert_cell(cur_x, cur_y);
  cursor_visible = 1;
}

// Hide cursor
static void cursor_hide(void) {
  if (!cursor_enabled || !cursor_visible) return;
  invert_cell(cur_x, cur_y);
  cursor_visible = 0;
}

// Enable or disable cursor display
void fbcon_enable_cursor(int enable) {
  if (enable) {
    cursor_enabled = 1;
    cursor_show();
  } else {
    cursor_hide();
    cursor_enabled = 0;
  }
}

// Clear framebuffer console
void fbcon_clear(void) {
  if (!fb) return;
  for (uint32_t y = 0; y < fb_h; y++)
    for (uint32_t x = 0; x < fb_w; x++)
      fb[y * fb_ppsl + x] = bg;

  cur_x = 0; cur_y = 0;
  cursor_visible = 0;
  cursor_show();
}

// Print character to framebuffer console
void fbcon_putc(char c) {
  if (!fb) return;

  cursor_hide();

  if (c == '\r') { cursor_show(); return; }

  if (c == '\n') {
    cur_x = 0;
    cur_y++;
    uint32_t rows = fb_h / CHAR_H;
    if (cur_y >= rows) scroll();
    cursor_show();
    return;
  }

  if (c == '\b') {
    if (cur_x > 0) {
      cur_x--;
    }
    draw_char(cur_x, cur_y, ' ');
    cursor_show();
    return;
  }

  uint32_t cols = fb_w / CHAR_W;
  uint32_t rows = fb_h / CHAR_H;

  draw_char(cur_x, cur_y, c);
  cur_x++;

  if (cur_x >= cols) {
    cur_x = 0;
    cur_y++;
    if (cur_y >= rows) scroll();
  }

  cursor_show();
}

// Print string to framebuffer console
void fbcon_puts(const char *s) {
  for (; s && *s; s++) fbcon_putc(*s);
}