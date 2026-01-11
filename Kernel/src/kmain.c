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
#include <carlos/disk.h>
#include <carlos/part.h>
#include <carlos/fs.h>

/* 
  TODO: implement DEBUGGING levels and use PMM_LOG() instead of kprintf() here
  so that we can turn off verbose PMM logging when not needed.
*/

 /* ---------- Global state ---------- */ 
static BootInfo g_bi;              // kernel-owned copy
static const BootInfo *g_bip = 0;  // pointer used by the rest of the kernel

static Fs g_rootfs;               // root filesystem
static int g_rootfs_ready = 0;    // set to 1 if mounted successfully

void acpi_probe(const BootInfo *bi);

__attribute__((noreturn))
void kmain(BootInfo* bi){
  klog_init();
  kprintf("CarlKernel: hello from kernel!\n");
  kprintf("KERNEL BUILD: %s %s\n", __DATE__, __TIME__);
  kprintf("klog: level=%u mask=0x%08x\n", (unsigned)g_klog_level, (unsigned)g_klog_mask);

  gdt_init();
  kprintf("GDT/TSS: OK\n");

  idt_init();
  kprintf("IDT: OK\n");

  // ---- Validate BootInfo pointer + ABI first (do NOT deref bi before this) ----
  if (!bi) {
    kprintf("BootInfo: NULL\n");
    for(;;) __asm__ volatile ("hlt");
  }

  if (bi->magic != CARLOS_BOOTINFO_MAGIC) {
    kprintf("BootInfo: BAD magic=0x%llx\n", (unsigned long long)bi->magic);
    for(;;) __asm__ volatile ("hlt");
  }

  if (bi->abi_version != CARLOS_ABI_VERSION) {
    kprintf("BootInfo: ABI mismatch (kernel=%u bootloader=%u)\n",
            CARLOS_ABI_VERSION, bi->abi_version);
    for(;;) __asm__ volatile ("hlt");
  }

  if (bi->memdesc_size == 0 ||
      bi->memmap == 0 || bi->memmap_size == 0 ||
      (bi->memmap_size % bi->memdesc_size) != 0) {
    kprintf("BootInfo: bad memory map info (memmap=0x%llx size=%llu desc=%llu)\n",
            (unsigned long long)bi->memmap,
            (unsigned long long)bi->memmap_size,
            (unsigned long long)bi->memdesc_size);
    for(;;) __asm__ volatile ("hlt");
  }

  // ---- Snapshot BootInfo once; use g_bip everywhere else ----
  g_bi  = *bi;
  g_bip = &g_bi;

  kprintf("BootInfo: OK\n");
  kprintf("BI: magic=0x%llx bootinfo_phys=0x%llx\n",
          (unsigned long long)g_bip->magic,
          (unsigned long long)g_bip->bootinfo_phys);

  kprintf("FB: base=0x%llx size=0x%llx %ux%u ppsl=%u fmt=%u\n",
          (unsigned long long)g_bip->fb_base,
          (unsigned long long)g_bip->fb_size,
          g_bip->fb_width, g_bip->fb_height,
          g_bip->fb_ppsl, g_bip->fb_format);

  // enable FB logging only after BootInfo is trusted
  klog_enable_fb(g_bip);
  kprintf("Framebuffer: %ux%u ppsl=%u fmt=%u base=%p\n",
          g_bip->fb_width, g_bip->fb_height,
          g_bip->fb_ppsl, g_bip->fb_format,
          phys_to_ptr(g_bip->fb_base));

  extern unsigned char __kernel_start;
  extern unsigned char __kernel_end;
  kprintf("Kernel range: %p - %p\n", &__kernel_start, &__kernel_end);

  kprintf("BootInfo.bootinfo_phys = %p\n", phys_to_cptr(g_bip->bootinfo_phys));
  kprintf("BootInfo.memmap        = %p\n", phys_to_cptr(g_bip->memmap));
  kprintf("BootInfo.memmap_size   = %llu\n", (unsigned long long)g_bip->memmap_size);
  kprintf("BootInfo.memdesc_size  = %llu\n", (unsigned long long)g_bip->memdesc_size);
  kprintf("BootInfo.memdesc_ver   = %u\n", g_bip->memdesc_ver);

  acpi_probe(g_bip);

  int rc = pci_init();
  kprintf("PCI: init rc=%d\n", rc);

  pmm_init(g_bip);
  kprintf("PMM free pages = %llu\n", (unsigned long long)pmm_free_count());

  rc = time_init();
  kprintf("TIME: init rc=%d\n", rc);

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

  // ---- Mount root filesystem ----
  rc = fs_mount_root(&g_rootfs, g_bip);  
  kprintf("FS: mount_root rc=%d\n", rc);
  g_rootfs_ready = (rc == 0);

  kapi_bind_fs(&g_rootfs);
  

  shell_run(g_bip, g_rootfs_ready ? &g_rootfs : NULL);

  for(;;) __asm__ volatile ("hlt");
}