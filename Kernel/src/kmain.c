#include <stdint.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/pmm.h>
#include <carlos/kmem.h>
#include <carlos/shell.h>
#include <carlos/klog.h>
#include <carlos/acpi.h>
#include <carlos/pci.h>
#include <carlos/kapi.h>
#include <carlos/gdt.h>
#include <carlos/idt.h>
#include <carlos/time.h>

static BootInfo g_bi;              // kernel-owned copy
static const BootInfo *g_bip = 0;  // pointer used by the rest of the kernel


void acpi_probe(const BootInfo *bi);

__attribute__((noreturn))
void kmain(BootInfo* bi){
  klog_init();
  kprintf("CarlKernel: hello from kernel!\n");
  kprintf("KERNEL BUILD: %s %s\n", __DATE__, __TIME__);

  extern void hexdump_128(const void *p); // write a tiny version of your hexdump_512
  kprintf("bi ptr=%p\n", bi);
  hexdump_128(bi);

  gdt_init();
  kprintf("GDT/TSS: OK\n");

  idt_init();
  kprintf("IDT: OK\n");

  kprintf("BI: magic=0x%llx layout=0x%llx bootinfo_phys=0x%llx\n",
        bi->magic, bi->bootinfo_phys);
  kprintf("FB: base=0x%llx size=0x%llx %ux%u ppsl=%u fmt=%u\n",
        bi->fb_base, bi->fb_size, bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format);

  klog_enable_fb(bi);
  kprintf("Framebuffer: %ux%u ppsl=%u fmt=%u base=%p\n",
        bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format,
        phys_to_ptr(bi->fb_base));

  // Validate BootInfo struct from bootloader 
  // (important for stability/security)
  // Check magic number
  if (!bi || bi->magic != CARLOS_BOOTINFO_MAGIC) {
    kprintf("BootInfo: BAD\n");
    for(;;) __asm__ volatile ("hlt");
  }

  // Check ABI version
  if (bi->abi_version != CARLOS_ABI_VERSION) {
    kprintf("BootInfo: ABI version mismatch (kernel=%u bootloader=%u)\n",
            CARLOS_ABI_VERSION, bi->abi_version);
    for(;;) __asm__ volatile ("hlt");
  }

  // Check memory map info
  if (bi->memdesc_size == 0 ||
      bi->memmap == 0 || bi->memmap_size == 0 ||
      (bi->memmap_size % bi->memdesc_size) != 0) {
    kprintf("BootInfo: bad memory map info\n");
    for(;;) __asm__ volatile ("hlt");
  }

  extern unsigned char __kernel_start;
  extern unsigned char __kernel_end;

  // Copy once, then forget the original pointer
  g_bi = *bi;
  g_bip = &g_bi;

  kprintf("BootInfo: OK\n");
  kprintf("Kernel range: %p - %p\n", &__kernel_start, &__kernel_end);

  kprintf("BootInfo.bootinfo = %p\n", phys_to_cptr(g_bip->bootinfo_phys));
  kprintf("BootInfo.memmap   = %p\n", phys_to_cptr(g_bip->memmap));
  kprintf("BootInfo.memmap_size  = %llu\n", (unsigned long long)g_bip->memmap_size);
  kprintf("BootInfo.memdesc_size = %llu\n", (unsigned long long)g_bip->memdesc_size);
  kprintf("BootInfo.memdesc_ver  = %u\n", g_bip->memdesc_ver);
  acpi_probe(g_bip);

  pci_init();
  
  time_init();

  pmm_init(g_bip);
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

  shell_run(g_bip);

  for(;;) __asm__ volatile ("hlt");
}