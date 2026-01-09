#pragma once
#include <Uefi.h>
#include <Guid/Acpi.h>

EFI_PHYSICAL_ADDRESS FindAcpiRsdp(EFI_SYSTEM_TABLE *ST, UINT32 *OutRev);