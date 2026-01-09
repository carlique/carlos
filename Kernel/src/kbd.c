#include <stdint.h>
#include <carlos/kbd.h>

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_CMD    0x64

static inline void outb(uint16_t port, uint8_t val){ __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }

static int shift_down = 0;

// Set 1 scancode maps (enough for a shell)
static const char map[128] = {
  [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',
  [0x0C]='-',[0x0D]='=',
  [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
  [0x1A]='[',[0x1B]=']',
  [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
  [0x27]=';',[0x28]='\'',[0x29]='`',
  [0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',
  [0x33]=',',[0x34]='.',[0x35]='/',
  [0x39]=' ',
};

static const char map_shift[128] = {
  [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',
  [0x0C]='_',[0x0D]='+',
  [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',[0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',
  [0x1A]='{',[0x1B]='}',
  [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',[0x24]='J',[0x25]='K',[0x26]='L',
  [0x27]=':',[0x28]='"',[0x29]='~',
  [0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',[0x31]='N',[0x32]='M',
  [0x33]='<',[0x34]='>',[0x35]='?',
  [0x39]=' ',
};

static void wait_input_clear(void){
  // status bit1 = input buffer full
  for (int i=0;i<100000;i++) if ((inb(KBD_STATUS) & 0x02) == 0) return;
}

static void wait_output_full(void){
  // status bit0 = output buffer full
  for (int i=0;i<100000;i++) if ((inb(KBD_STATUS) & 0x01) != 0) return;
}

static void flush_output(void){
  while (inb(KBD_STATUS) & 0x01) (void)inb(KBD_DATA);
}

static void kbd_write_cmd(uint8_t cmd){
  wait_input_clear();
  outb(KBD_CMD, cmd);
}

static void kbd_write_data(uint8_t data){
  wait_input_clear();
  outb(KBD_DATA, data);
}

static uint8_t kbd_read_data(void){
  wait_output_full();
  return inb(KBD_DATA);
}

void kbd_init(void){
  flush_output();

  // Enable keyboard interface
  kbd_write_cmd(0xAE);

  // Enable scanning on the keyboard device
  kbd_write_data(0xF4);
  (void)kbd_read_data(); // ACK (0xFA) typically; ignore for now

  shift_down = 0;
}

int kbd_try_getc(char *out){
  if ((inb(KBD_STATUS) & 0x01) == 0) return 0;

  uint8_t sc = inb(KBD_DATA);

  // Handle extended prefix (ignore for now)
  if (sc == 0xE0) return 0;

  // Key release? (high bit set)
  int released = (sc & 0x80) != 0;
  uint8_t code = sc & 0x7F;

  // Shift press/release
  if (code == 0x2A || code == 0x36) { // LSHIFT/RSHIFT
    shift_down = released ? 0 : 1;
    return 0;
  }

  if (released) return 0;

  // Special keys we care about
  if (code == 0x1C) { *out = '\n'; return 1; }   // Enter
  if (code == 0x0E) { *out = 0x08; return 1; }   // Backspace

  char ch = shift_down ? map_shift[code] : map[code];
  if (!ch) return 0;

  *out = ch;
  return 1;
}