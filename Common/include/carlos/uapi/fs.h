#pragma once
#include <carlos/uapi/types.h>

#define CARLOS_NAME_MAX 64

typedef struct CarlosDirEnt {
  char name[CARLOS_NAME_MAX]; // null-terminated
  u32  size;                  // bytes (0 for dirs)
  u8   type;                  // 1=file, 2=dir
  u8   _pad[3];
} CarlosDirEnt;