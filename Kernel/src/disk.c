#include <carlos/disk.h>

int disk_read(Disk *d, uint64_t lba, uint32_t count, void *buf){
  if (!d || !d->read || !buf || count == 0) return -1;
  return d->read(d, lba, count, buf);
}