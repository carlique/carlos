#pragma once
#include <stdint.h>

#pragma pack(push,1)

typedef struct {
  char     Signature[8];   // "RSD PTR "
  uint8_t  Checksum;
  char     OemId[6];
  uint8_t  Revision;       // 0 for ACPI 1.0, 2 for ACPI 2.0+
  uint32_t RsdtAddress;    // valid if Revision==0
  // ACPI 2.0+ fields exist after this, but not needed for v1
} AcpiRsdpV1;

typedef struct {
  char     Signature[4];
  uint32_t Length;
  uint8_t  Revision;
  uint8_t  Checksum;
  char     OemId[6];
  char     OemTableId[8];
  uint32_t OemRevision;
  uint32_t CreatorId;
  uint32_t CreatorRevision;
} AcpiSdtHeader;

// MADT = SDT header + LAPIC address + flags + entries...
typedef struct {
  AcpiSdtHeader Hdr;
  uint32_t      LapicAddress;
  uint32_t      Flags;
  uint8_t       Entries[];
} AcpiMadt;

// MADT entry header
typedef struct {
  uint8_t Type;
  uint8_t Length;
} AcpiMadtEntryHdr;

// Type 0: Processor Local APIC
typedef struct {
  AcpiMadtEntryHdr H;
  uint8_t  AcpiProcessorId;
  uint8_t  ApicId;
  uint32_t Flags; // bit0 = enabled
} AcpiMadtLocalApic;

// Type 1: IO APIC
typedef struct {
  AcpiMadtEntryHdr H;
  uint8_t  IoApicId;
  uint8_t  Reserved;
  uint32_t IoApicAddress;
  uint32_t GlobalSystemInterruptBase;
} AcpiMadtIoApic;

// Type 2: Interrupt Source Override
typedef struct {
  AcpiMadtEntryHdr H;
  uint8_t  Bus;        // 0 = ISA
  uint8_t  SourceIrq;  // ISA IRQ
  uint32_t Gsi;        // global system interrupt
  uint16_t Flags;      // polarity/trigger
} AcpiMadtIso;

// Type 5: Local APIC Address Override (ACPI 2.0+, but can appear)
typedef struct {
  AcpiMadtEntryHdr H;
  uint16_t Reserved;
  uint64_t LapicAddress;
} AcpiMadtLapicAddrOverride;

const void* acpi_find_sdt(const char sig4[4]);

#pragma pack(pop)