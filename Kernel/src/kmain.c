#include <stdint.h>
#include <carlos/bootinfo.h>
#include <carlos/pmm.h>
#include <carlos/kmem.h>
#include <carlos/shell.h>
#include <carlos/klog.h>
#include <carlos/acpi.h>
#include <carlos/kapi.h>
#include <carlos/gdt.h>
#include <carlos/idt.h>

void acpi_probe(const BootInfo *bi);

__attribute__((noreturn))
void kmain(BootInfo* bi){
  klog_init();
  kprintf("CarlKernel: hello from kernel!\n");

  gdt_init();
  kprintf("GDT/TSS: OK\n");

  idt_init();
  kprintf("IDT: OK\n");
      
  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) {
    kprintf("BootInfo: BAD\n");
    for(;;) __asm__ volatile ("hlt");
  }

  klog_enable_fb(bi);
  kprintf("Framebuffer: %ux%u ppsl=%u fmt=%u base=%p\n",
        bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format,
        (void*)(uintptr_t)bi->fb_base);

  extern unsigned char __kernel_start;
  extern unsigned char __kernel_end;

  kprintf("BootInfo: OK\n");
  kprintf("Kernel range: %p - %p\n", &__kernel_start, &__kernel_end);

  kprintf("BootInfo.bootinfo = %p\n", (void*)(uintptr_t)bi->bootinfo);
  kprintf("BootInfo.memmap   = %p\n", (void*)(uintptr_t)bi->memmap);
  kprintf("BootInfo.memmap_size  = %llu\n", (unsigned long long)bi->memmap_size);
  kprintf("BootInfo.memdesc_size = %llu\n", (unsigned long long)bi->memdesc_size);
  kprintf("BootInfo.memdesc_ver  = %u\n", bi->memdesc_ver);

  acpi_probe(bi);

  pmm_init(bi);
  kprintf("PMM free pages = %llu\n", (unsigned long long)pmm_free_count());

  kmem_init();

  kprintf("Carlos ABI v%u\n", g_api.abi_version);

  // Quick heap sanity (optional)
  char *buf = (char*)kmalloc(64);
  if (!buf) {
    kprintf("kmalloc failed\n");
    for(;;) __asm__ volatile ("hlt");
  }
  for (int i = 0; i < 63; i++) buf[i] = 'A' + (i % 26);
  buf[63] = 0;
  kprintf("kmalloc ok: %p\n", buf);

  shell_run(bi);

  for(;;) __asm__ volatile ("hlt");
}