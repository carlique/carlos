#include <carlos/disk.h>
#include <stdint.h>

// your existing AHCI entry
int ahci_read(uint32_t port, uint64_t lba, uint32_t count, void *buf);

typedef struct {
  uint32_t port;
} AhciDiskCtx;

static int ahci_disk_read(Disk *d, uint64_t lba, uint32_t count, void *buf){
  if (!d || !d->ctx) return -1;
  AhciDiskCtx *c = (AhciDiskCtx*)d->ctx;
  return ahci_read(c->port, lba, count, buf);
}

// simplest: static ctx (one disk). If you want multiple disks later, kmalloc it.
static AhciDiskCtx g_ctx;

int disk_init_ahci(Disk *out, uint32_t port){
  if (!out) return -1;
  g_ctx.port = port;

  out->sector_size = 512;        // your AHCI path assumes 512 today
  out->read = ahci_disk_read;
  out->ctx  = &g_ctx;
  return 0;
}