#include <stdint.h>
#include <carlos/part.h>
#include <carlos/disk.h>

typedef struct __attribute__((packed)) {
  uint8_t  status;
  uint8_t  chs_first[3];
  uint8_t  type;
  uint8_t  chs_last[3];
  uint32_t lba_start;
  uint32_t lba_count;
} MbrPart;

typedef struct __attribute__((packed)) {
  uint8_t  boot_code[446];
  MbrPart  part[4];
  uint16_t sig; // 0xAA55
} Mbr;

int part_mbr_get(Disk *d, int index, Partition *out) {
  if (!d || !out || index < 0 || index > 3) return -1;

  uint8_t sec[512];
  int rc = disk_read(d, 0, 1, sec);
  if (rc != 0) return rc;

  // signature bytes at end of sector
  if (sec[510] != 0x55 || sec[511] != 0xAA) {
    return -2; // no MBR (superfloppy)
  }

  const Mbr *m = (const Mbr*)sec;
  const MbrPart *p = &m->part[index];
  if (p->type == 0 || p->lba_count == 0) return -3;

  out->lba_start = p->lba_start;
  out->lba_count = p->lba_count;
  out->type      = p->type;
  return 0;
}

int part_find_fat_candidate(Disk *d, Partition *out) {
  if (!d || !out) return -1;

  // If there is no MBR, treat the whole disk as one “partition” at LBA0.
  // fat16_mount only needs base_lba anyway.
  Partition p0;
  int rc0 = part_mbr_get(d, 0, &p0);
  if (rc0 == -2) {
    PART_INFO("part: no MBR signature -> superfloppy (LBA0)\n");
    out->lba_start = 0;
    out->lba_count = 0;   // unknown / unused by FAT mount for now
    out->type      = 0;   // "raw"
    return 0;
  }

  // Common FAT16 partition types: 0x04, 0x06, 0x0E
  for (int i = 0; i < 4; i++) {
    Partition p;
    int rc = part_mbr_get(d, i, &p);
    if (rc != 0) continue;

    PART_DBG("part: mbr[%d] type=0x%x lba=%llu cnt=%llu\n",
             i, p.type, (unsigned long long)p.lba_start, (unsigned long long)p.lba_count);

    if (p.type == 0x04 || p.type == 0x06 || p.type == 0x0E) {
      *out = p;
      return 0;
    }
  }

  return -2;
}