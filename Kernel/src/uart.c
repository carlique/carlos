#include <stdint.h>
#include <carlos/uart.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val){ __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }

/** Simple serial port initialization and output functions for debugging
    (COM1 at 0x3F8)
    disable interrupts
    enable DLAB (so divisor registers are accessible)
    set baud divisor (here 0x0003)
    8 data bits, no parity, 1 stop bit
    FIFO enable/clear
    modem control bits
    based on https://wiki.osdev.org/Serial_Ports
**/
void uart_init(void){
  outb(COM1+1,0x00); outb(COM1+3,0x80); outb(COM1+0,0x03); outb(COM1+1,0x00);
  outb(COM1+3,0x03); outb(COM1+2,0xC7); outb(COM1+4,0x0B);
}

int uart_try_getc(char *out){
  // LSR bit 0 = data ready
  if ((inb(COM1+5) & 0x01) == 0) return 0;
  *out = (char)inb(COM1+0);
  return 1;
}

void uart_putc(char c){
  while ((inb(COM1+5) & 0x20) == 0) { }
  outb(COM1, (uint8_t)c);
}

void uart_puts(const char *s){
  for (; *s; s++){
    if (*s == '\n') uart_putc('\r');
    uart_putc(*s);
  }
}