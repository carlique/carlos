// Common/include/carlos/uapi/api.h
#pragma once
#include <carlos/uapi/types.h>
#include <carlos/uapi/fs.h>
#include <carlos/uapi/abi.h>

typedef struct CarlosApi {
  u32 abi_version;

  // Console I/O (stdout-like)
  s32 (*write)(const char *buf, size_t len);

  // Process control (later: real exit to scheduler)
  void (*exit)(s32 code);

  // Memory (early: page-based or simple heap behind the scenes)
  void* (*alloc_pages)(size_t pages);
  void  (*free_pages)(void *p, size_t pages);

  // NEW (ABI bump): list directory entries into user buffer
  // returns: >=0 number of entries, <0 error
  s32 (*fs_listdir)(const char *path, CarlosDirEnt *ents, u32 max_ents);
} CarlosApi;