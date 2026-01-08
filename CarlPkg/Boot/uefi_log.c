#include <Uefi.h>

#include <Library/UefiLib.h>          // Print()
#include <Library/PrintLib.h>         // UnicodeVSPrint()
#include <Library/SerialPortLib.h>    // SerialPortInitialize(), SerialPortWrite()
#include <Library/BaseLib.h>          // VA_LIST
#include <stdarg.h>

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

    // Make terminal-friendly CRLF
    if (c == L'\n') {
      if (j < sizeof(Buf) - 2) {
        Buf[j++] = '\r';
        Buf[j++] = '\n';
      }
      continue;
    }

    // Basic ASCII-only conversion (good enough for early boot logs)
    if (c < 0x80) {
      Buf[j++] = (CHAR8)c;
    } else {
      Buf[j++] = '?';
    }
  }

  Buf[j] = '\0';
  SerialPortWrite((UINT8 *)Buf, j);
}

VOID Logf(IN CONST CHAR16 *Fmt, ...)
{
  if (Fmt == NULL) return;

  // Format once into a Unicode buffer
  CHAR16 WBuf[1024];

  VA_LIST Args;
  VA_START(Args, Fmt);
  UnicodeVSPrint(WBuf, sizeof(WBuf), Fmt, Args);
  VA_END(Args);

  // Console
  Print(L"%s", WBuf);

  // Serial
  SerialWriteFromUnicode(WBuf);
}
