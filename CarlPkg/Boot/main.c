/*
  CarlOs.c — minimal pure-UEFI app (study project)

  - Prints to UEFI console (QEMU window)
  - Writes to serial (QEMU terminal with `-serial mon:stdio`)
  - Waits for a key so it doesn’t immediately return to the boot menu
*/

#include <Uefi.h>

#include <Library/UefiLib.h>                  // Print()
#include <Library/UefiBootServicesTableLib.h> // gBS, gST
#include <Library/SerialPortLib.h>            // SerialPortInitialize(), SerialPortWrite()

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  // 1) UEFI console output
  Print(L"CarlOs: UEFI console says hello!\r\n");

  // 2) Serial output (COM1)
  SerialPortInitialize();
  CONST CHAR8 Msg[] = "CarlOs: serial says hello!\r\n";
  SerialPortWrite((UINT8 *)Msg, sizeof(Msg) - 1);

  // 3) Wait for a key
  Print(L"Press any key to exit...\r\n");
  UINTN Index = 0;
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);

  EFI_INPUT_KEY Key;
  gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

  Print(L"Bye.\r\n");
  return EFI_SUCCESS;
}
