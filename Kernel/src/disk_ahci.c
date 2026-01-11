#include <carlos/disk.h>
#include <carlos/ahci.h>
#include <carlos/klog.h>

#define AHCI_DBG(...)  KLOG(KLOG_MOD_AHCI, KLOG_DBG,  __VA_ARGS__)
#define AHCI_ERR(...)  KLOG(KLOG_MOD_AHCI, KLOG_ERR,  __VA_ARGS__)

typedef struct {
  uint32_t port;
} DiskAhciCtx;

static int disk_ahci_read(Disk *d, uint64_t lba, uint32_t count, void *buf){
  if (!d || !d->ctx) return -1;
  DiskAhciCtx *c = (DiskAhciCtx*)d->ctx;
  return ahci_read(c->port, lba, count, buf);
}

static int disk_ahci_write(Disk *d, uint64_t lba, uint32_t count, const void *buf){
  if (!d || !d->ctx) return -1;
  DiskAhciCtx *c = (DiskAhciCtx*)d->ctx;
  return ahci_write(c->port, lba, count, buf);
}

int disk_init_ahci(Disk *out, uint32_t port){
  static DiskAhciCtx ctxs[32];
  if (!out) return -1;
  if (port >= 32) return -2;

  ctxs[port].port = port;

  *out = (Disk){
    .sector_size = 512,
    .read  = disk_ahci_read,
    .write = disk_ahci_write,
    .ctx   = &ctxs[port],
  };

  AHCI_DBG("disk: ahci init port=%u sector=512\n", port);
  return 0;
}