#pragma once
#include <stdint.h>

typedef struct Disk Disk;
struct Disk {
  uint32_t sector_size;
  int (*read)(Disk*, uint64_t lba, uint32_t count, void *buf);
  int (*write)(Disk*, uint64_t lba, uint32_t count, const void *buf);
  void *ctx;
};

int disk_init_ahci(Disk *out, uint32_t port);
int disk_read(Disk *d, uint64_t lba, uint32_t count, void *buf);
int disk_write(Disk *d, uint64_t lba, uint32_t count, const void *buf);