#pragma once
#include <Uefi.h>

VOID LogInit(VOID);
VOID Logf(IN CONST CHAR16 *Fmt, ...);
