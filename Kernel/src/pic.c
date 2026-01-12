// src/pic.c
#include <carlos/pic.h>

static inline void outb(uint16_t port, uint8_t v){
  __asm__ volatile ("outb %0, %1" :: "a"(v), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port){
  uint8_t v;
  __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
  return v;
}
static inline void io_wait(void){
  outb(0x80, 0);
}

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define ICW1_INIT  0x10
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01

void pic_init(uint8_t off_master, uint8_t off_slave){
  uint8_t a1 = inb(PIC1_DATA);
  uint8_t a2 = inb(PIC2_DATA);

  outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();
  outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4); io_wait();

  outb(PIC1_DATA, off_master); io_wait();
  outb(PIC2_DATA, off_slave);  io_wait();

  outb(PIC1_DATA, 4); io_wait();  // slave at IRQ2
  outb(PIC2_DATA, 2); io_wait();

  outb(PIC1_DATA, ICW4_8086); io_wait();
  outb(PIC2_DATA, ICW4_8086); io_wait();

  // restore masks (caller can override)
  outb(PIC1_DATA, a1);
  outb(PIC2_DATA, a2);
}

void pic_set_mask(uint8_t irq, int masked){
  uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  uint8_t  line = (irq < 8) ? irq : (irq - 8);

  uint8_t v = inb(port);
  if (masked) v |=  (uint8_t)(1u << line);
  else        v &= (uint8_t)~(1u << line);
  outb(port, v);
}

void pic_eoi(uint8_t irq){
  // If from slave, EOI slave first, then master
  if (irq >= 8) outb(PIC2_CMD, 0x20);
  outb(PIC1_CMD, 0x20);
}