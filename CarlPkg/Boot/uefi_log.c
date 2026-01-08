#include <Uefi.h>

#include <Library/UefiLib.h>       // Print()
#include <Library/SerialPortLib.h> // SerialPortInitialize(), SerialPortWrite()

static BOOLEAN gSerialReady = FALSE;

VOID LogInit(VOID)
{
  if (!gSerialReady) {
    SerialPortInitialize();
    gSerialReady = TRUE;
  }
}

static VOID SerialWriteFromUnicode(IN CONST CHAR16 *W)
{
  if (!gSerialReady || W == NULL) return;

  CHAR8 Buf[1024];
  UINTN j = 0;

  for (UINTN i = 0; W[i] != L'\0' && j < sizeof(Buf) - 1; i++) {
    CHAR16 c = W[i];

    if (c == L'\n') {
      if (j < sizeof(Buf) - 2) { Buf[j++] = '\r'; Buf[j++] = '\n'; }
      continue;
    }

    Buf[j++] = (c < 0x80) ? (CHAR8)c : '?';
  }

  SerialPortWrite((UINT8 *)Buf, j);
}

VOID Log(IN CONST CHAR16 *Msg)
{
  if (Msg == NULL) return;

  Print(L"%s", Msg);
  SerialWriteFromUnicode(Msg);
}