// Kernel/include/carlos/kapi.h
#pragma once
#include <carlos/uapi/api.h>
#include <carlos/fs.h>

extern const CarlosApi g_api;
void kapi_bind_fs(Fs *fs);

// cwd in FAT-style path83, without leading slash, e.g. "" (root), "BIN", "SYS/CFG"
void kapi_set_cwd(const char *path83);