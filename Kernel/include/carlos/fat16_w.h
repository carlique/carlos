#pragma once
#ifndef CARLOS_FS_INTERNAL
# error "fat16_w.h is internal to fs.c"
#endif
#include <carlos/fat16.h>
#include <carlos/fat16.h>

int fat16_alloc_clus(Fat16 *fs, uint16_t *out_clus);
int fat16_mkdir_path83(Fat16 *fs, const char *path83);