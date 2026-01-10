// include/carlos/part.h
#pragma once
#include <stdint.h>
#include <carlos/disk.h>

typedef struct Partition {
  uint64_t lba_start;
  uint64_t lba_count;
  uint8_t  type;       // MBR type when MBR; 0 for GPT (unused)
} Partition;

// MBR
int part_mbr_get(const Disk *d, int index, Partition *out);
int part_find_fat_candidate(const Disk *d, Partition *out);

// GPT: find partition by PARTUUID (GPT Partition GUID)
int part_gpt_find_by_partuuid(const Disk *d, const char *uuid_str, Partition *out);