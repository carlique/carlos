#include <carlos/disk.h>
#include <carlos/klog.h>

#define DISK_ERR(...)  KLOG(KLOG_MOD_DISK, KLOG_ERR,  __VA_ARGS__)

int disk_read(Disk *d, uint64_t lba, uint32_t count, void *buf){
  if (!d || !d->read || !buf) return -1;
  if (count == 0) return 0;
  return d->read(d, lba, count, buf);
}

int disk_write(Disk *d, uint64_t lba, uint32_t count, const void *buf){
  if (!d || !d->write || !buf) return -1;
  if (count == 0) return 0;
  return d->write(d, lba, count, buf);
}