#include <stdint.h>
#include <carlos/bootinfo.h>
#include <carlos/pmm.h>
#include <carlos/kmem.h>
#include <carlos/uart.h>
#include <carlos/shell.h>

static void uart_put_hex_u64(uint64_t v) {
  static const char *hex = "0123456789ABCDEF";
  uart_puts("0x");
  for (int i = 60; i >= 0; i -= 4) {
    uart_putc(hex[(v >> i) & 0xF]);
  }
}

static void uart_put_dec_u64(uint64_t v) {
  // optional: small decimal printer (handy later)
  char buf[21];
  int i = 0;
  if (v == 0) { uart_putc('0'); return; }
  while (v > 0 && i < (int)sizeof(buf)) {
    buf[i++] = '0' + (v % 10);
    v /= 10;
  }
  for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
}

__attribute__((noreturn))
void kmain(BootInfo* bi){
  uart_init();
  shell_run(bi);

  uart_puts("CarlKernel: hello from kernel!\n");

  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) {
    uart_puts("BootInfo: BAD\n");
  } else {
    uart_puts("BootInfo: OK\n");

    extern unsigned char __kernel_start;
    extern unsigned char __kernel_end;

    uart_puts("Kernel range: ");
    uart_put_hex_u64((uint64_t)(uintptr_t)&__kernel_start);
    uart_puts(" - ");
    uart_put_hex_u64((uint64_t)(uintptr_t)&__kernel_end);
    uart_puts("\n");

    uart_puts("BootInfo.memmap = ");
    uart_put_hex_u64(bi->memmap);
    uart_puts("\nBootInfo.memmap_size = ");
    uart_put_dec_u64(bi->memmap_size);
    uart_puts("\nBootInfo.memdesc_size = ");
    uart_put_dec_u64(bi->memdesc_size);
    uart_puts("\n");
    // (optional) print descriptor count only
    uint64_t count = bi->memmap_size / bi->memdesc_size;
    uart_puts("MemMap descriptors: ");
    uart_put_dec_u64(count);
    uart_puts("\n");
  }

  pmm_init(bi);
  uart_puts("PMM free pages = ");
  uart_put_dec_u64(pmm_free_count());
  uart_puts("\n");

  kmem_init();

  char *buf = (char*)kmalloc(64);
  if (!buf) {
    uart_puts("kmalloc failed\n");
    for(;;) __asm__("hlt");
  }

  // Fill it with something and print (ASCII)
  for (int i = 0; i < 63; i++) buf[i] = 'A' + (i % 26);
  buf[63] = 0;

  uart_puts("kmalloc ok: ");
  uart_put_hex_u64((uint64_t)(uintptr_t)buf);
  uart_puts("\n");

  void *pages[8];

  for (int i = 0; i < 8; i++) {
    pages[i] = pmm_alloc_page();
    uart_puts("alloc "); uart_put_dec_u64(i);
    uart_puts(" = "); uart_put_hex_u64((uint64_t)(uintptr_t)pages[i]);
    uart_puts("\n");
    if (!pages[i]) { uart_puts("PMM FAIL\n"); for(;;) __asm__("hlt"); }

    // write pattern
    uint64_t *q = (uint64_t*)pages[i];
    q[0] = 0x1111111111111111ULL + (uint64_t)i;
    q[1] = 0x2222222222222222ULL + (uint64_t)i;
  }

  for (int i = 0; i < 8; i++) {
    pmm_free_page(pages[i]);
  }

  uart_puts("PMM test: freed 8 pages\n");

  for(;;) __asm__ volatile ("hlt");
}