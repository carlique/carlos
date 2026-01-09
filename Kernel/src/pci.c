#include <stdint.h>
#include <carlos/klog.h>
#include <carlos/acpi.h>
#include <carlos/pci.h>

// MCFG table (ACPI "MCFG") describes PCI Express ECAM windows.
typedef struct __attribute__((packed)) {
  AcpiSdtHeader Hdr;
  uint64_t Reserved;
  uint8_t  Entries[];
} AcpiMcfg;

typedef struct __attribute__((packed)) {
  uint64_t BaseAddress;   // ECAM base physical address
  uint16_t PciSegment;
  uint8_t  StartBus;
  uint8_t  EndBus;
  uint32_t Reserved;
} McfgAlloc;

static uint64_t g_ecam_base = 0;
static uint8_t  g_bus_start = 0;
static uint8_t  g_bus_end   = 0;
static uint16_t g_seg       = 0;

static inline uint32_t mmio_read32(uint64_t addr){
  return *(volatile uint32_t*)(uintptr_t)addr;
}

static uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  // ECAM: base + bus*1MB + dev*32KB + fun*4KB + off
  uint64_t addr = g_ecam_base
    + ((uint64_t)bus << 20)
    + ((uint64_t)dev << 15)
    + ((uint64_t)fun << 12)
    + (uint64_t)(off & 0xFFC);
  return mmio_read32(addr);
}

static uint16_t pci_cfg_read16(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  uint32_t v = pci_cfg_read32(bus, dev, fun, off & 0xFFC);
  uint16_t sh = (off & 2) ? 16 : 0;
  return (uint16_t)((v >> sh) & 0xFFFF);
}

static uint8_t pci_cfg_read8(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  uint32_t v = pci_cfg_read32(bus, dev, fun, off & 0xFFC);
  uint16_t sh = (uint16_t)((off & 3) * 8);
  return (uint8_t)((v >> sh) & 0xFF);
}

int pci_init(void){
  const AcpiMcfg *mcfg = (const AcpiMcfg*)acpi_find_sdt("MCFG");
  if (!mcfg){
    kprintf("PCI: MCFG not found\n");
    return -1;
  }

  uint32_t len = mcfg->Hdr.Length;
  if (len < sizeof(AcpiMcfg)){
    kprintf("PCI: MCFG too small\n");
    return -2;
  }

  uint32_t bytes = len - (uint32_t)sizeof(AcpiMcfg);
  uint32_t n = bytes / (uint32_t)sizeof(McfgAlloc);
  const McfgAlloc *a = (const McfgAlloc*)mcfg->Entries;

  kprintf("PCI: MCFG entries=%u\n", n);
  if (n == 0) return -3;

  // For now: pick the first allocation entry
  g_ecam_base = a[0].BaseAddress;
  g_seg       = a[0].PciSegment;
  g_bus_start = a[0].StartBus;
  g_bus_end   = a[0].EndBus;

  kprintf("PCI: ECAM base=%p seg=%u buses=%u..%u\n",
          (void*)(uintptr_t)g_ecam_base, g_seg, g_bus_start, g_bus_end);

  return 0;
}

void pci_list(void){
  if (!g_ecam_base){
    kprintf("PCI: not initialized\n");
    return;
  }

  for (uint16_t bus = g_bus_start; bus <= g_bus_end; bus++){
    for (uint8_t dev = 0; dev < 32; dev++){
      for (uint8_t fun = 0; fun < 8; fun++){
        uint32_t id = pci_cfg_read32((uint8_t)bus, dev, fun, 0x00);
        uint16_t vendor = (uint16_t)(id & 0xFFFF);
        if (vendor == 0xFFFF) {
          if (fun == 0) break; // no function 0 => no device
          continue;
        }
        uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);

        uint32_t classr = pci_cfg_read32((uint8_t)bus, dev, fun, 0x08);
        uint8_t class_code = (uint8_t)(classr >> 24);
        uint8_t subclass   = (uint8_t)(classr >> 16);
        uint8_t prog_if    = (uint8_t)(classr >> 8);

        kprintf("%02x:%02x.%u  ven=%04x dev=%04x  class=%02x:%02x pi=%02x\n",
                (uint32_t)bus, (uint32_t)dev, (uint32_t)fun,
                vendor, device, class_code, subclass, prog_if);

        // If not multifunction, stop at function 0
        uint32_t hdr = pci_cfg_read32((uint8_t)bus, dev, fun, 0x0C);
        uint8_t header_type = (uint8_t)((hdr >> 16) & 0xFF);
        if (fun == 0 && ((header_type & 0x80) == 0)) break;
      }
    }
    }
}

int pci_dump_bdf(uint8_t bus, uint8_t dev, uint8_t fun){
if (!g_ecam_base) { kprintf("PCI: not initialized\n"); return -1; }

uint32_t id = pci_cfg_read32(bus, dev, fun, 0x00);
uint16_t vendor = (uint16_t)(id & 0xFFFF);
if (vendor == 0xFFFF) { kprintf("PCI: no device at %02x:%02x.%u\n", bus, dev, fun); return -2; }
uint16_t device = (uint16_t)(id >> 16);

uint16_t command = pci_cfg_read16(bus, dev, fun, 0x04);
uint16_t status  = pci_cfg_read16(bus, dev, fun, 0x06);

uint32_t classr = pci_cfg_read32(bus, dev, fun, 0x08);
uint8_t class_code = (uint8_t)(classr >> 24);
uint8_t subclass   = (uint8_t)(classr >> 16);
uint8_t prog_if    = (uint8_t)(classr >> 8);
uint8_t rev        = (uint8_t)(classr >> 0);

uint8_t header_type = pci_cfg_read8(bus, dev, fun, 0x0E);
uint8_t irq_line    = pci_cfg_read8(bus, dev, fun, 0x3C);
uint8_t irq_pin     = pci_cfg_read8(bus, dev, fun, 0x3D);

kprintf("PCI %02x:%02x.%u\n", bus, dev, fun);
kprintf("  id: %04x:%04x\n", vendor, device);
kprintf("  class: %02x:%02x pi=%02x rev=%02x\n", class_code, subclass, prog_if, rev);
kprintf("  header: 0x%02x %s\n", (uint32_t)header_type, (header_type & 0x80) ? "(multifunction)" : "");
kprintf("  cmd=0x%04x status=0x%04x\n", (uint32_t)command, (uint32_t)status);
kprintf("  irq: line=%u pin=%u\n", (uint32_t)irq_line, (uint32_t)irq_pin);

// BARs (type 0 header only)
if ((header_type & 0x7F) == 0x00) {
    for (int i = 0; i < 6; i++) {
    uint16_t off = (uint16_t)(0x10 + i * 4);
    uint32_t bar = pci_cfg_read32(bus, dev, fun, off);

    if (bar == 0) {
        kprintf("  BAR%d: 0\n", (uint32_t)i);
        continue;
    }

    if (bar & 1) {
        // I/O space
        uint32_t port = bar & ~3u;
        kprintf("  BAR%d: IO  port=0x%08x\n", (uint32_t)i, port);
    } else {
        // MMIO
        uint32_t type = (bar >> 1) & 3u;
        uint64_t addr = (uint64_t)(bar & ~0xFu);

        if (type == 2) {
        // 64-bit BAR uses next BAR as high dword
        uint32_t bar_hi = pci_cfg_read32(bus, dev, fun, (uint16_t)(off + 4));
        addr |= ((uint64_t)bar_hi << 32);
        kprintf("  BAR%d: MMIO64 addr=%p\n", (uint32_t)i, (void*)(uintptr_t)addr);
        i++; // consumed next BAR
        } else {
        kprintf("  BAR%d: MMIO32 addr=%p\n", (uint32_t)i, (void*)(uintptr_t)addr);
        }
    }
    }
} else {
    kprintf("  BARs: (non-type0 header, skipping)\n");
}

return 0;
}

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  return pci_cfg_read32(bus, dev, fun, off);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  uint32_t v = pci_cfg_read32(bus, dev, fun, off & 0xFFC);
  uint16_t sh = (off & 2) ? 16 : 0;
  return (uint16_t)((v >> sh) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off){
  uint32_t v = pci_cfg_read32(bus, dev, fun, off & 0xFFC);
  uint16_t sh = (uint16_t)((off & 3) * 8);
  return (uint8_t)((v >> sh) & 0xFF);
}

int pci_get_bus_range(uint8_t *start, uint8_t *end){
  if (!g_ecam_base) return -1;
  if (start) *start = g_bus_start;
  if (end)   *end   = g_bus_end;
  return 0;
}