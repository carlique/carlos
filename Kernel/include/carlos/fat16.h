#pragma once
#include <stdint.h>
#include <carlos/disk.h>

typedef struct Fat16 {
  Disk    *disk;
  uint64_t base_lba;

  uint16_t bps;        // bytes per sector (expect 512)
  uint8_t  spc;        // sectors per cluster
  uint16_t rsvd;
  uint8_t  nfats;
  uint16_t root_ent;
  uint32_t fatsz;      // sectors per FAT

  uint64_t fat_lba;
  uint64_t root_lba;
  uint32_t root_secs;
  uint64_t data_lba;
} Fat16;

// Directory iterator
typedef struct FatDirIter {
  Fat16   *fs;

  uint8_t  in_root;         // 1=root dir (contiguous), 0=cluster chain
  uint8_t  done;

  // root iteration
  uint32_t root_entries_left;

  // subdir iteration (cluster chain)
  uint16_t cur_clus;
  uint8_t  sec_in_clus;

  // sector buffer
  uint64_t cur_lba;
  uint16_t ent_idx;         // 0..(bps/32-1) within current sector
  uint8_t  secbuf[512];
} FatDirIter;

int fat16_mount(Fat16 *fs, Disk *disk, uint64_t base_lba);

int fat16_root_iter_begin(Fat16 *fs, FatDirIter *it);
int fat16_dir_iter_begin(Fat16 *fs, FatDirIter *it, uint16_t first_clus);
int fat16_dir_iter_next(FatDirIter *it, /*out*/ char name83[13],
                        /*out*/ uint8_t *attr,
                        /*out*/ uint16_t *clus,
                        /*out*/ uint32_t *size);

int fat16_read_file_by_clus(Fat16 *fs, uint16_t first_clus,
                            uint32_t offset, uint32_t size, void *out);

int fat16_stat_path83(Fat16 *fs, const char *path,
                      /*out*/ uint16_t *clus,
                      /*out*/ uint8_t  *attr,
                      /*out*/ uint32_t *size);
