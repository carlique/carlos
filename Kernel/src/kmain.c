#include <stdint.h>
#include <carlos/bootinfo.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val){ __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port){ uint8_t r; __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port)); return r; }

static void serial_init(void){
  outb(COM1+1,0x00); outb(COM1+3,0x80); outb(COM1+0,0x03); outb(COM1+1,0x00);
  outb(COM1+3,0x03); outb(COM1+2,0xC7); outb(COM1+4,0x0B);
}
static void serial_putc(char c){ while ((inb(COM1+5)&0x20)==0){} outb(COM1,(uint8_t)c); }
static void serial_puts(const char* s){ for(;*s;s++){ if(*s=='\n') serial_putc('\r'); serial_putc(*s);} }

__attribute__((noreturn))
void kmain(BootInfo* bi){
  serial_init();
  serial_puts("CarlKernel: hello from kernel!\n");
  if (bi && bi->magic == CARLOS_BOOTINFO_MAGIC) serial_puts("BootInfo: OK\n");
  for(;;) __asm__ volatile ("hlt");
}