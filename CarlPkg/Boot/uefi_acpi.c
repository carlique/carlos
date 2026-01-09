#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Library/BaseMemoryLib.h>   // CompareGuid

EFI_PHYSICAL_ADDRESS FindAcpiRsdp(EFI_SYSTEM_TABLE *ST, UINT32 *OutRev) {
  if (OutRev) *OutRev = 0;

  for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE *T = &ST->ConfigurationTable[i];

    if (CompareGuid(&T->VendorGuid, &gEfiAcpi20TableGuid)) {
      if (OutRev) *OutRev = 2;
      return (EFI_PHYSICAL_ADDRESS)(UINTN)T->VendorTable;
    }
    if (CompareGuid(&T->VendorGuid, &gEfiAcpi10TableGuid)) {
      if (OutRev) *OutRev = 1;
      return (EFI_PHYSICAL_ADDRESS)(UINTN)T->VendorTable;
    }
  }
  return 0;
}