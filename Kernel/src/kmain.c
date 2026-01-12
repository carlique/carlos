// src/kmain.c
#include <stdint.h>
#include <carlos/boot/bootinfo.h>

#include <carlos/klog.h>

#include <carlos/gdt.h>
#include <carlos/idt.h>

#include <carlos/acpi.h>
#include <carlos/pci.h>

#include <carlos/pmm.h>
#include <carlos/kmem.h>

#include <carlos/intr.h>
#include <carlos/pic.h>

#include <carlos/time.h>

#include <carlos/fs.h>
#include <carlos/kapi.h>
#include <carlos/shell.h>

/* ---------- Global state ---------- */
static BootInfo g_bi;              // kernel-owned copy
static const BootInfo *g_bip = 0;  // pointer used by the rest of the kernel

static Fs  g_rootfs;
static int g_rootfs_ready = 0;

/* ---------- Boot logging helpers ---------- */

static int      g_boot_time_ok = 0;
static uint64_t g_boot_t0_ns   = 0;

static inline uint64_t rd_rflags(void){
  uint64_t r;
  __asm__ volatile ("pushfq; popq %0" : "=r"(r));
  return r;
}

__attribute__((noreturn))
static void boot_halt(const char *msg){
  if (msg) kprintf("PANIC: %s\n", msg);
  __asm__ volatile ("cli");
  for(;;) __asm__ volatile ("hlt");
}

static void boot_stamp_prefix(void){
  if (!g_boot_time_ok) {
    kprintf("[BOOT] ");
    return;
  }

  uint64_t now = time_now_ns();
  uint64_t d   = (now > g_boot_t0_ns) ? (now - g_boot_t0_ns) : 0;
  uint64_t ms  = d / 1000000ull;

  // roughly linux-ish: [    0.123]
  kprintf("[ %5llu.%03llu ] ", (unsigned long long)(ms / 1000ull),
                               (unsigned long long)(ms % 1000ull));
}

#define BOOT_PRINT(...) do { boot_stamp_prefix(); kprintf(__VA_ARGS__); } while(0)

static void boot_print_banner(void){
  BOOT_PRINT("CarlKernel: hello from kernel!\n");
  BOOT_PRINT("Build: %s %s\n", __DATE__, __TIME__);
  BOOT_PRINT("klog: level=%u mask=0x%08x\n",
             (unsigned)g_klog_level, (unsigned)g_klog_mask);
}

static void boot_validate_bootinfo(const BootInfo *bi){
  if (!bi) boot_halt("BootInfo: NULL");

  if (bi->magic != CARLOS_BOOTINFO_MAGIC) {
    kprintf("BootInfo: BAD magic=0x%llx\n", (unsigned long long)bi->magic);
    boot_halt("BootInfo: bad magic");
  }

  if (bi->abi_version != CARLOS_ABI_VERSION) {
    kprintf("BootInfo: ABI mismatch (kernel=%u bootloader=%u)\n",
            CARLOS_ABI_VERSION, bi->abi_version);
    boot_halt("BootInfo: ABI mismatch");
  }

  if (bi->memdesc_size == 0 ||
      bi->memmap == 0 || bi->memmap_size == 0 ||
      (bi->memmap_size % bi->memdesc_size) != 0) {
    kprintf("BootInfo: bad memmap (memmap=0x%llx size=%llu desc=%llu)\n",
            (unsigned long long)bi->memmap,
            (unsigned long long)bi->memmap_size,
            (unsigned long long)bi->memdesc_size);
    boot_halt("BootInfo: bad memory map");
  }
}

static void boot_snapshot_bootinfo(const BootInfo *bi){
  g_bi  = *bi;
  g_bip = &g_bi;
}

static void boot_print_bootinfo_summary(const BootInfo *bi){
  BOOT_PRINT("BootInfo: OK (abi=%u)\n", bi->abi_version);

  BOOT_PRINT("FB: %ux%u ppsl=%u fmt=%u base=0x%llx size=0x%llx\n",
             bi->fb_width, bi->fb_height, bi->fb_ppsl, bi->fb_format,
             (unsigned long long)bi->fb_base, (unsigned long long)bi->fb_size);

  BOOT_PRINT("memmap: phys=0x%llx size=%llu desc=%llu ver=%u\n",
             (unsigned long long)bi->memmap,
             (unsigned long long)bi->memmap_size,
             (unsigned long long)bi->memdesc_size,
             (unsigned)bi->memdesc_ver);

  extern unsigned char __kernel_start;
  extern unsigned char __kernel_end;
  BOOT_PRINT("kernel: %p - %p\n", &__kernel_start, &__kernel_end);
}

/* forward decl from your acpi code */
void acpi_probe(const BootInfo *bi);

__attribute__((noreturn))
void kmain(BootInfo *bi)
{
  // ---------- early console/log ----------
  klog_init();
  boot_print_banner();

  // ---------- early CPU tables ----------
  gdt_init();
  BOOT_PRINT("cpu: GDT/TSS: OK\n");

  idt_init();
  BOOT_PRINT("cpu: IDT: OK\n");

  // ---------- bootinfo validation + snapshot ----------
  boot_validate_bootinfo(bi);
  boot_snapshot_bootinfo(bi);

  // enable FB logging only after BootInfo is trusted
  klog_enable_fb(g_bip);
  boot_print_bootinfo_summary(g_bip);

  // ---------- firmware/platform discovery ----------
  BOOT_PRINT("acpi: probe...\n");
  acpi_probe(g_bip);

  int rc = pci_init();
  BOOT_PRINT("pci: init rc=%d\n", rc);

  // ---------- physical + heap memory ----------
  pmm_init(g_bip);
  BOOT_PRINT("mm: PMM free pages=%llu\n", (unsigned long long)pmm_free_count());

  kmem_init();
  BOOT_PRINT("mm: heap: OK\n");

  // optional heap sanity (keep but quieter)
  {
    char *buf = (char*)kmalloc(64);
    if (!buf) boot_halt("kmalloc failed");
    for (int i = 0; i < 63; i++) buf[i] = 'A' + (i % 26);
    buf[63] = 0;
    BOOT_PRINT("mm: kmalloc sanity: OK (%p)\n", buf);
  }

  // ---------- timebase ----------
  rc = time_init();
  BOOT_PRINT("time: init rc=%d\n", rc);

  // If HPET is working, start timestamped boot logs from here
  if (rc == 0) {
    g_boot_time_ok = 1;
    g_boot_t0_ns   = time_now_ns();
    BOOT_PRINT("time: hpet: OK\n");
  } else {
    BOOT_PRINT("time: hpet: unavailable (still OK for now)\n");
  }

  // ---------- interrupts (PIC + IRQ stubs) ----------
  intr_init();            // install IDT gates 0x20..0x2F
  pic_init(0x20, 0x28);   // remap PIC

  // Unmask what you want enabled initially
  pic_set_mask(0, 0);     // timer
  pic_set_mask(1, 0);     // keyboard

  // Start PIT + hook IRQ0 handler (your time_timer_start does irq_register + pit_init_hz(1000))
  time_timer_start();

  intr_enable();
  BOOT_PRINT("intr: enabled (RFLAGS=0x%llx IF=%d)\n",
             (unsigned long long)rd_rflags(),
             (int)((rd_rflags() >> 9) & 1));

  // ---------- filesystem + ABI ----------
  BOOT_PRINT("abi: Carlos ABI v%u\n", g_api.abi_version);

  rc = fs_mount_root(&g_rootfs, g_bip);
  g_rootfs_ready = (rc == 0);
  BOOT_PRINT("fs: mount_root rc=%d\n", rc);

  // Always bind; kapi code can decide how to behave if not ready
  kapi_bind_fs(&g_rootfs);

  // ---------- handoff to shell ----------
  BOOT_PRINT("shell: starting\n");
  shell_run(g_bip, g_rootfs_ready ? &g_rootfs : NULL);

  boot_halt("shell returned");
}