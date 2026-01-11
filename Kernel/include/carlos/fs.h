#pragma once
#include <stdint.h>
#include <carlos/disk.h>
#include <carlos/part.h>
#include <carlos/fat16.h>
#include <carlos/boot/bootinfo.h>

typedef struct Fs {
  Disk      disk;
  Partition root_part;
  Fat16     fat;
  uint32_t  port;    // AHCI port index we mounted from
} Fs;

enum {
  FS_TYPE_NONE = 0,
  FS_TYPE_FILE = 1,
  FS_TYPE_DIR  = 2,
};

typedef struct FsStat {
  uint8_t  type;   // FS_TYPE_*
  uint32_t size;
} FsStat;

int fs_mount_esp(Fs *out);                      // picks partition + mounts FAT16
int fs_mount_root(Fs *out, const BootInfo *bi); // from BootInfo root_spec

int fs_read_file(Fs *fs, const char *path, void **out_buf, uint32_t *out_size);
int fs_read_file_at(Fs *fs, const char *path,
                    uint32_t offset, void *buf, uint32_t len,
                    uint32_t *out_read);

int fs_list_dir(Fs *fs, const char *path);
// new: generic listdir that hides FAT16
typedef int (*fs_listdir_cb)(void *ud, const char name83[13], uint8_t attr, uint32_t size);
int fs_listdir(Fs *fs, const char *path, fs_listdir_cb cb, void *ud);

int fs_mkdir(Fs *fs, const char *path);
int fs_stat(Fs *fs, const char *path, FsStat *st);
