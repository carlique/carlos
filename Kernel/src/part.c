// Kernel/src/part.c
#include <carlos/part.h>
#include <carlos/disk.h>
#include <stdint.h>

#define MBR_SIG_OFF 510
#define MBR_SIG_0   0x55
#define MBR_SIG_1   0xAA
#define MBR_PT_OFF  446
#define MBR_PT_ENT  16

typedef struct __attribute__((packed)) {
  uint8_t  status;
  uint8_t  chs_first[3];
  uint8_t  type;
  uint8_t  chs_last[3];
  uint32_t lba_start;
  uint32_t lba_count;
} MbrEntry;

int part_mbr_get(Disk *d, int index, Partition *out){
  if (!d || !out || index < 0 || index > 3) return -1;

  uint8_t sec[512];
  int rc = disk_read((Disk*)d, 0, 1, sec);
  if (rc != 0) return -2;

  if (sec[MBR_SIG_OFF] != MBR_SIG_0 || sec[MBR_SIG_OFF+1] != MBR_SIG_1) return -3;

  const MbrEntry *e = (const MbrEntry*)(sec + MBR_PT_OFF + index * MBR_PT_ENT);

  if (e->type == 0 || e->lba_count == 0) return -4;

  out->lba_start = (uint64_t)e->lba_start;
  out->lba_count = (uint64_t)e->lba_count;
  out->type      = e->type;
  return 0;
}

static int is_fat_candidate(uint8_t type){
  // FAT16 + FAT32 common MBR types
  switch (type){
    case 0x06: // FAT16
    case 0x0E: // FAT16 LBA
    case 0x0B: // FAT32
    case 0x0C: // FAT32 LBA
      return 1;
    default:
      return 0;
  }
}

int part_find_fat_candidate(Disk *d, Partition *out){
  if (!d || !out) return -1;

  for (int i = 0; i < 4; i++){
    Partition p;
    int rc = part_mbr_get(d, i, &p);
    if (rc != 0) continue;
    if (is_fat_candidate(p.type)){
      *out = p;
      return 0;
    }
  }
  return -2;
}