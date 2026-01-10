#include <stdint.h>
#include <carlos/hpet.h>
#include <carlos/klog.h>
#include <carlos/phys.h>
#include <carlos/acpi.h>   // we’ll add/find acpi_find_sdt("HPET")

// Generic Address Structure (GAS)
typedef struct __attribute__((packed)) {
  uint8_t  AddressSpaceId;
  uint8_t  RegisterBitWidth;
  uint8_t  RegisterBitOffset;
  uint8_t  AccessSize;
  uint64_t Address;
} AcpiGas;

// HPET table
typedef struct __attribute__((packed)) {
  AcpiSdtHeader Hdr;
  uint32_t   EventTimerBlockId;
  AcpiGas    BaseAddress;
  uint8_t    HpetNumber;
  uint16_t   MinClockTick;
  uint8_t    PageProtection;
} AcpiHpet;

// HPET registers
typedef struct __attribute__((packed)) {
  volatile uint64_t GCAP_ID;   // 0x00
  volatile uint64_t _rsv0;     // 0x08
  volatile uint64_t GCONF;     // 0x10
  volatile uint64_t _rsv1;     // 0x18
  volatile uint64_t GIS;       // 0x20
  uint8_t _pad0[0xF0 - 0x28];
  volatile uint64_t MAIN;      // 0xF0
} HpetRegs;

static volatile HpetRegs *g_hpet = 0;   // mapped pointer (via phys_to_ptr for now)
static uint64_t  g_hpet_phys = 0;        // physical base
static uint64_t  g_fs_per_tick = 0; // femtoseconds per tick
static uint64_t  g_hz = 0;          // HPET frequency (ticks per second)

static inline uint64_t rd_main(void){
  if (!g_hpet) return 0;
  return g_hpet->MAIN;
}

int hpet_init_from_acpi(void){
  const AcpiHpet *t = (const AcpiHpet*)acpi_find_sdt("HPET");
  if (!t) {
    kprintf("HPET: table not found\n");
    return -1;
  }

  if (t->BaseAddress.AddressSpaceId != 0 /* SystemMemory */) {
    kprintf("HPET: unsupported GAS space=%u\n", t->BaseAddress.AddressSpaceId);
    return -2;
  }

  if (t->BaseAddress.Address == 0) return -5;

  uint64_t base = t->BaseAddress.Address;
  g_hpet_phys = base;
  g_hpet = (volatile HpetRegs*)phys_to_ptr(base);

  uint64_t cap = g_hpet->GCAP_ID;
  g_fs_per_tick = (cap >> 32); // HPET period in femtoseconds

  if (g_fs_per_tick == 0) {
    kprintf("HPET: bad period\n");
    return -3;
  }

  // freq = 10^15 / (fs_per_tick)
  g_hz = 1000000000000000ULL / g_fs_per_tick;
  if (g_hz == 0) {
    kprintf("HPET: bad hz\n");
    return -4;
  }

  // Disable, reset main counter, enable
  g_hpet->GCONF = 0;
  g_hpet->MAIN  = 0;
  g_hpet->GCONF = 1;

  kprintf("HPET: base=0x%llx fs/tick=%llu hz=%llu\n", 
          g_hpet_phys, g_fs_per_tick, g_hz);
  return 0;
}

uint64_t hpet_now_ns(void){
  if (!g_hpet || g_hz == 0) return 0;

  // ns = ticks * 1e9 / hz
  uint64_t ticks = rd_main();
  
  // Avoid overflow: split ticks into quotient+remainder
  uint64_t q = ticks / g_hz;
  uint64_t r = ticks % g_hz;

  // q seconds in ns + fractional part
  uint64_t ns = q * 1000000000ULL;
  ns += (r * 1000000000ULL) / g_hz;
  return ns;
}