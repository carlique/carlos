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

int part_mbr_get(const Disk *d, int index, Partition *out) {
  if (!d || !out || index < 0 || index > 3) return -1;

  uint8_t sec[512];
  int rc = disk_read((Disk*)d, 0, 1, sec);
  if (rc != 0) return rc;

  const Mbr *m = (const Mbr*)sec;

  // No MBR signature -> treat as “superfloppy” (filesystem starts at LBA0)
  if (m->sig != 0xAA55) {
    return -2;
  }

  const MbrPart *p = &m->part[index];
  if (p->type == 0 || p->lba_count == 0) return -3;

  out->lba_start = p->lba_start;
  out->lba_count = p->lba_count;
  out->type      = p->type;
  return 0;
}

int part_find_fat_candidate(const Disk *d, Partition *out) {
  if (!d || !out) return -1;

  // Common FAT16 partition types: 0x04, 0x06, 0x0E
  for (int i = 0; i < 4; i++) {
    Partition p;
    int rc = part_mbr_get(d, i, &p);
    if (rc != 0) continue;

    if (p.type == 0x04 || p.type == 0x06 || p.type == 0x0E) {
      *out = p;
      return 0;
    }
  }

  return -2;
}