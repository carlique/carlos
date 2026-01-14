#include <stdint.h>
#include <stddef.h>
#include <carlos/phys.h>
#include <carlos/acpi.h>
#include <carlos/boot/bootinfo.h>
#include <carlos/klog.h>

#define ACPI_MOD KLOG_MOD_CORE

// Levels:
// - high-level success/failure => INFO/WARN/ERR
// - table listings and MADT dump => DBG/TRACE
#define ACPI_INFO(...)  KLOG(ACPI_MOD, KLOG_INFO,  __VA_ARGS__)
#define ACPI_WARN(...)  KLOG(ACPI_MOD, KLOG_WARN,  __VA_ARGS__)
#define ACPI_ERR(...)   KLOG(ACPI_MOD, KLOG_ERR,   __VA_ARGS__)
#define ACPI_DBG(...)   KLOG(ACPI_MOD, KLOG_DBG,   __VA_ARGS__)
#define ACPI_TRACE(...) KLOG(ACPI_MOD, KLOG_TRACE, __VA_ARGS__)

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
  // Only used in error paths; keep as kprintf since it writes to current log sink.
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
  // MADT dump is verbose; keep at DBG/TRACE
  ACPI_DBG("acpi: MADT @ %p len=%u LAPIC=0x", (void*)madt, madt->Hdr.Length);
  khex_u32(madt->LapicAddress);
  ACPI_DBG(" flags=0x");
  khex_u32(madt->Flags);
  ACPI_DBG("\n");

  const uint8_t *p = madt->Entries;
  const uint8_t *end = ((const uint8_t*)madt) + madt->Hdr.Length;

  while (p + sizeof(AcpiMadtEntryHdr) <= end){
    const AcpiMadtEntryHdr *h = (const AcpiMadtEntryHdr*)p;
    if (h->Length < sizeof(AcpiMadtEntryHdr)) break;
    if (p + h->Length > end) break;

    switch (h->Type) {
      case 0: {
        const AcpiMadtLocalApic *e = (const AcpiMadtLocalApic*)p;
        ACPI_TRACE("  MADT: LAPIC cpu=%u apic=%u flags=0x",
                   e->AcpiProcessorId, e->ApicId);
        khex_u32(e->Flags);
        ACPI_TRACE(" %s\n", (e->Flags & 1) ? "EN" : "DIS");
        break;
      }
      case 1: {
        const AcpiMadtIoApic *e = (const AcpiMadtIoApic*)p;
        ACPI_TRACE("  MADT: IOAPIC id=%u addr=0x", e->IoApicId);
        khex_u32(e->IoApicAddress);
        ACPI_TRACE(" gsi_base=0x");
        khex_u32(e->GlobalSystemInterruptBase);
        ACPI_TRACE("\n");
        break;
      }
      case 2: {
        const AcpiMadtIso *e = (const AcpiMadtIso*)p;
        ACPI_TRACE("  MADT: ISO bus=%u irq=%u -> gsi=%u flags=0x",
                   e->Bus, e->SourceIrq, e->Gsi);
        khex_u16(e->Flags);
        ACPI_TRACE("\n");
        break;
      }
      case 5: {
        const AcpiMadtLapicAddrOverride *e = (const AcpiMadtLapicAddrOverride*)p;
        ACPI_TRACE("  MADT: LAPIC_ADDR_OVERRIDE = 0x%llx\n",
                   (uint64_t)e->LapicAddress);
        break;
      }
      default:
        ACPI_TRACE("  MADT: type=%u len=%u (ignored)\n", h->Type, h->Length);
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
      if (h->Length < sizeof(AcpiSdtHeader)) continue;
      if (!checksum_ok(h, h->Length)) continue;
      return h;
    }
  }

  return 0;
}

void acpi_probe(const BootInfo *bi){
  if (!bi || !bi->acpi_rsdp){
    ACPI_WARN("acpi: no RSDP\n");
    return;
  }

  const AcpiRsdpV1 *rsdp = phys_to_cptr(bi->acpi_rsdp);

  ACPI_INFO("acpi: RSDP=0x%llx guid_kind=%u rsdp_rev=%u\n",
            (unsigned long long)bi->acpi_rsdp,
            (unsigned)bi->acpi_guid_kind,
            (unsigned)rsdp->Revision);

  if (!sig8_eq(rsdp->Signature, "RSD PTR ")){
    ACPI_ERR("acpi: bad RSDP signature\n");
    return;
  }

  // ACPI 1.0 checksum covers first 20 bytes
  if (!checksum_ok(rsdp, 20)){
    ACPI_ERR("acpi: RSDP checksum FAIL\n");
    return;
  }

  if (rsdp->Revision != 0){
    ACPI_WARN("acpi: RSDP is v2+; XSDT not implemented yet\n");
    return;
  }

  const AcpiSdtHeader *rsdt = phys_to_cptr(rsdp->RsdtAddress);

  ACPI_DBG("acpi: RSDT @ 0x%llx\n", (unsigned long long)rsdp->RsdtAddress);

  if (!sig4_eq(rsdt->Signature, "RSDT")){
    ACPI_ERR("acpi: RSDT sig mismatch: ");
    print_sig4(rsdt->Signature);
    ACPI_ERR("\n");
    return;
  }

  if (rsdt->Length < sizeof(AcpiSdtHeader)){
    ACPI_ERR("acpi: RSDT length too small\n");
    return;
  }

  if (!checksum_ok(rsdt, rsdt->Length)){
    ACPI_ERR("acpi: RSDT checksum FAIL\n");
    return;
  }

  uint32_t entry_count = (rsdt->Length - sizeof(AcpiSdtHeader)) / 4;
  const uint32_t *ent = (const uint32_t*)((const uint8_t*)rsdt + sizeof(AcpiSdtHeader));

  g_rsdt = rsdt;
  g_rsdt_ent = ent;
  g_rsdt_count = entry_count;

  ACPI_DBG("acpi: RSDT entries=%u\n", (unsigned)entry_count);

  const AcpiMadt *madt = 0;

  for (uint32_t i=0;i<entry_count;i++){
    uint64_t phys = (uint64_t)ent[i];
    const AcpiSdtHeader *h = phys_to_cptr(phys);

    // table listing is noisy => DBG
    ACPI_DBG("acpi: [%u] ", (unsigned)i);
    print_sig4(h->Signature);
    ACPI_DBG(" @ 0x%llx len=%u\n", (unsigned long long)phys, (unsigned)h->Length);

    if (sig4_eq(h->Signature, "APIC")){
      madt = (const AcpiMadt*)h;
    }
  }

  if (!madt){
    ACPI_WARN("acpi: MADT (APIC) not found\n");
    return;
  }

  if (!checksum_ok(madt, madt->Hdr.Length)){
    ACPI_ERR("acpi: MADT checksum FAIL\n");
    return;
  }

  madt_dump(madt);
}