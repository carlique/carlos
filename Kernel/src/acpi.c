#include <stdint.h>
#include <stddef.h>
#include <carlos/phys.h>
#include <carlos/acpi.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/klog.h>

static const AcpiSdtHeader *g_rsdt = 0;
static const uint32_t      *g_rsdt_ent = 0;
static uint32_t             g_rsdt_count = 0;

static int sig8_eq(const char s[8], const char *lit){
  for (int i=0;i<8;i++) if (s[i] != lit[i]) return 0;
  return 1;
}

static int sig4_eq(const char s[4], const char *lit){
  return s[0]==lit[0] && s[1]==lit[1] && s[2]==lit[2] && s[3]==lit[3];
}

static void print_sig4(const char s[4]){
  kprintf("%c%c%c%c", s[0], s[1], s[2], s[3]);
}

static int checksum_ok(const void *p, uint32_t len){
  const uint8_t *b = (const uint8_t*)p;
  uint8_t sum = 0;
  for (uint32_t i=0;i<len;i++) sum = (uint8_t)(sum + b[i]);
  return sum == 0;
}

static void khex_u32(uint32_t v){
  static const char *h = "0123456789ABCDEF";
  for (int i = 7; i >= 0; i--) {
    kputc(h[(v >> (i * 4)) & 0xF]);
  }
}

static void khex_u16(uint16_t v){
  static const char *h = "0123456789ABCDEF";
  for (int i = 3; i >= 0; i--) {
    kputc(h[(v >> (i * 4)) & 0xF]);
  }
}

static void madt_dump(const AcpiMadt *madt){
  kprintf("ACPI: MADT @ %p len=%u LAPIC=0x", (void*)madt, madt->Hdr.Length);
  khex_u32(madt->LapicAddress);
  kprintf(" flags=0x");
  khex_u32(madt->Flags);
  kprintf("\n");

  const uint8_t *p = madt->Entries;
  const uint8_t *end = ((const uint8_t*)madt) + madt->Hdr.Length;

  while (p + sizeof(AcpiMadtEntryHdr) <= end){
    const AcpiMadtEntryHdr *h = (const AcpiMadtEntryHdr*)p;
    if (h->Length < sizeof(AcpiMadtEntryHdr)) break;
    if (p + h->Length > end) break;

    switch (h->Type) {
      case 0: {
        const AcpiMadtLocalApic *e = (const AcpiMadtLocalApic*)p;
  
        kprintf("  MADT: LAPIC cpu=%u apic=%u flags=0x", e->AcpiProcessorId, e->ApicId);
        khex_u32(e->Flags);
        kprintf(" %s\n", (e->Flags & 1) ? "EN" : "DIS");
        break;
      }
      case 1: {
        const AcpiMadtIoApic *e = (const AcpiMadtIoApic*)p;
        
        kprintf("  MADT: IOAPIC id=%u addr=0x", e->IoApicId);
        khex_u32(e->IoApicAddress);
        kprintf(" gsi_base=0x");
        khex_u32(e->GlobalSystemInterruptBase);
        kprintf("\n");
        break;
      }
      case 2: {
         const AcpiMadtIso *e = (const AcpiMadtIso*)p;
  
        kprintf("  MADT: ISO bus=%u irq=%u -> gsi=%u flags=0x",
                e->Bus, e->SourceIrq, e->Gsi);
        khex_u16(e->Flags);
        kprintf("\n");
        break;
      }
      case 5: {
        const AcpiMadtLapicAddrOverride *e = (const AcpiMadtLapicAddrOverride*)p;
        kprintf("  MADT: LAPIC_ADDR_OVERRIDE = 0x%llx\n", (uint64_t)e->LapicAddress);
        break;  
      }
      default:
        kprintf("  MADT: type=%u len=%u (ignored)\n", h->Type, h->Length);
        break;
    }

    p += h->Length;
  }
}

const AcpiSdtHeader* acpi_find_sdt(const char sig4[4]){
  if (!g_rsdt || !g_rsdt_ent || g_rsdt_count == 0) return 0;

  for (uint32_t i = 0; i < g_rsdt_count; i++) {
    uint64_t phys = (uint64_t)g_rsdt_ent[i];
    const AcpiSdtHeader *h = phys_to_cptr(phys);

    if (sig4_eq(h->Signature, sig4)) {
      // Optional safety: basic length + checksum
      if (h->Length < sizeof(AcpiSdtHeader)) continue;
      if (!checksum_ok(h, h->Length)) continue;
      return h;
    }
  }

  return 0;
}

void acpi_probe(const BootInfo *bi){
  if (!bi || !bi->acpi_rsdp){
    kprintf("ACPI: no RSDP\n");
    return;
  }

  const AcpiRsdpV1 *rsdp = phys_to_cptr(bi->acpi_rsdp);

  kprintf("ACPI: RSDP=0x%llx guid_kind=%u rsdp_rev=%u\n",
        bi->acpi_rsdp, bi->acpi_guid_kind, rsdp->Revision);

  if (!sig8_eq(rsdp->Signature, "RSD PTR ")){
    kprintf("ACPI: bad RSDP signature\n");
    return;
  }

  // ACPI 1.0 checksum covers first 20 bytes
  if (!checksum_ok(rsdp, 20)){
    kprintf("ACPI: RSDP checksum FAIL\n");
    return;
  }

  if (rsdp->Revision != 0){
    kprintf("ACPI: RSDP is v2+; add XSDT path later\n");
    return;
  }

  const AcpiSdtHeader *rsdt = phys_to_cptr(rsdp->RsdtAddress);

  kprintf("ACPI: RSDT @ 0x%llx\n", (uint64_t)rsdp->RsdtAddress);

  if (!sig4_eq(rsdt->Signature, "RSDT")){
    kprintf("ACPI: RSDT sig mismatch: ");
    print_sig4(rsdt->Signature);
    kprintf("\n");
    return;
  }

  if (rsdt->Length < sizeof(AcpiSdtHeader)){
    kprintf("ACPI: RSDT length too small\n");
    return;
  }

  if (!checksum_ok(rsdt, rsdt->Length)){
    kprintf("ACPI: RSDT checksum FAIL\n");
    return;
  }

  uint32_t entry_count = (rsdt->Length - sizeof(AcpiSdtHeader)) / 4;
  const uint32_t *ent = (const uint32_t*)((const uint8_t*)rsdt + sizeof(AcpiSdtHeader));

  g_rsdt = rsdt;
  g_rsdt_ent = ent;
  g_rsdt_count = entry_count;

  kprintf("ACPI: RSDT entries=%u\n", entry_count);

  const AcpiMadt *madt = 0;

  for (uint32_t i=0;i<entry_count;i++){
    uint64_t phys = (uint64_t)ent[i];
    const AcpiSdtHeader *h = phys_to_cptr(phys);

    kprintf("ACPI: [");
    kprintf("%u", i);
    kprintf("] ");
    print_sig4(h->Signature);
    kprintf(" @ 0x%llx len=%u\n", phys, h->Length);

    if (sig4_eq(h->Signature, "APIC")){
      madt = (const AcpiMadt*)h;
    }
  }

  if (!madt){
    kprintf("ACPI: MADT (APIC) not found\n");
    return;
  }

  if (!checksum_ok(madt, madt->Hdr.Length)){
    kprintf("ACPI: MADT checksum FAIL\n");
    return;
  }


  madt_dump(madt);
}