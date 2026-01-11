// fs.c
#include <stdint.h>
#include <stddef.h>

#include <carlos/fs.h>
#include <carlos/klog.h>
#include <carlos/kmem.h>

#include <carlos/part.h>
#include <carlos/fat16.h>
#include <carlos/fat16_w.h>   // only fs.c gets write access
#include <carlos/disk.h>

#define FAT_ATTR_DIR 0x10

static int streq(const char *a, const char *b){
  if (!a || !b) return 0;
  while (*a && *b){ if (*a++ != *b++) return 0; }
  return *a==0 && *b==0;
}

static int has_prefix(const char *s, const char *p){
  while (*p) { if (*s++ != *p++) return 0; }
  return 1;
}

static int is_root_path(const char *p){
  if (!p || !*p) return 1;
  if ((p[0] == '/' || p[0] == '\\') && p[1] == 0) return 1;
  return 0;
}

static void kputs_pad12(const char *s){
  int i = 0;
  for (; s && s[i] && i < 12; i++) kputc(s[i]);
  for (; i < 12; i++) kputc(' ');
}

// normalize:  "/efi/carlos" -> "EFI/CARLOS"
// also collapses leading slashes and converts '\' -> '/'
static void norm_path83(const char *in, char *out, size_t cap){
  size_t j = 0;
  if (!cap) return;
  out[0] = 0;

  // skip leading slashes
  while (in && (*in == '/' || *in == '\\')) in++;

  for (size_t i = 0; in && in[i] && j + 1 < cap; i++){
    char c = in[i];
    if (c == '\\') c = '/';

    // collapse duplicate '/'
    if (c == '/'){
      if (j == 0 || out[j-1] == '/') continue;
      out[j++] = '/';
      continue;
    }

    // uppercase for FAT 8.3 usage
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    out[j++] = c;
  }

  // trim trailing '/'
  if (j > 0 && out[j-1] == '/') j--;

  out[j] = 0;
}

int fs_mount_esp(Fs *out)
{
  if (!out) return -1;
  *out = (Fs){0};

  out->port = 0;

  int rc = disk_init_ahci(&out->disk, out->port);
  if (rc != 0) return rc;

  rc = part_find_fat_candidate(&out->disk, &out->root_part);
  if (rc != 0) return rc;

  rc = fat16_mount(&out->fat, &out->disk, out->root_part.lba_start);
  if (rc != 0) return rc;

  return 0;
}

int fs_list_dir(Fs *fs, const char *path)
{
  if (!fs) return -1;

  FatDirIter it;
  int rc = 0;

  if (is_root_path(path)) {
    rc = fat16_root_iter_begin(&fs->fat, &it);
    if (rc != 0) {
      kprintf("FS: fat16_root_iter_begin failed rc=%d\n", rc);
      return rc;
    }
    kprintf("DIR /\n");
  } else {
    char p83[256];
    norm_path83(path, p83, sizeof(p83));

    uint8_t  attr = 0;
    uint16_t clus = 0;
    uint32_t size = 0;

    rc = fat16_stat_path83(&fs->fat, p83, &clus, &attr, &size);
    if (rc != 0) {
      kprintf("FS: not found: %s (rc=%d)\n", p83, rc);
      return rc;
    }

    if ((attr & FAT_ATTR_DIR) == 0) {
      kprintf("FS: not a directory: %s\n", p83);
      return -2;
    }

    rc = fat16_dir_iter_begin(&fs->fat, &it, clus);
    if (rc != 0) {
      kprintf("FS: fat16_dir_iter_begin failed clus=%u rc=%d\n", (uint32_t)clus, rc);
      return rc;
    }

    kprintf("DIR %s (clus=%u)\n", p83, (uint32_t)clus);
  }

  for (;;) {
    char name83[13];
    uint8_t attr = 0;
    uint16_t clus = 0;
    uint32_t size = 0;

    rc = fat16_dir_iter_next(&it, name83, &attr, &clus, &size);
    if (rc > 0) break;        // end
    if (rc < 0) return rc;    // error

    // skip "." and ".."
    if (name83[0] == '.' && name83[1] == 0) continue;
    if (name83[0] == '.' && name83[1] == '.' && name83[2] == 0) continue;

    int is_dir = (attr & FAT_ATTR_DIR) != 0;
    kprintf("  %c ", is_dir ? 'D' : 'F');
    kputs_pad12(name83);
    kprintf("  clus=%u  size=%u\n", (uint32_t)clus, (uint32_t)size);
  }

  return 0;
}

int fs_mount_root(Fs *out, const BootInfo *bi){
  if (!out || !bi) return -1;
  *out = (Fs){0};

  kprintf("FS: mount_root: root_spec='%s'\n", bi->root_spec);

  const char *rs = bi->root_spec;
  if (!rs || rs[0] == 0 || streq(rs, "esp")) {
    kprintf("FS: root_spec empty/esp -> fallback FAT candidate on port0\n");

    out->port = 0;
    int rc = disk_init_ahci(&out->disk, out->port);
    kprintf("FS: disk_init_ahci(port0) rc=%d sector_size=%u\n", rc, out->disk.sector_size);
    if (rc != 0) return rc;

    rc = part_find_fat_candidate(&out->disk, &out->root_part);
    kprintf("FS: part_find_fat_candidate rc=%d lba=%llu count=%llu type=0x%x\n",
            rc, out->root_part.lba_start, out->root_part.lba_count, out->root_part.type);
    if (rc != 0) return rc;

    rc = fat16_mount(&out->fat, &out->disk, out->root_part.lba_start);
    kprintf("FS: fat16_mount rc=%d\n", rc);
    return rc;
  }

  if (has_prefix(rs, "partuuid=")) {
    const char *uuid = rs + 9;
    kprintf("FS: searching partuuid=%s\n", uuid);

    for (uint32_t port = 0; port < 32; port++){
      Disk d = (Disk){0};
      int rc = disk_init_ahci(&d, port);

      kprintf("FS: port %u disk_init rc=%d sector=%u\n", port, rc, d.sector_size);
      if (rc != 0) continue;

      Partition p = (Partition){0};
      rc = part_gpt_find_by_partuuid(&d, uuid, &p);

      kprintf("FS: port %u partuuid rc=%d lba=%llu count=%llu\n",
              port, rc, p.lba_start, p.lba_count);

      if (rc != 0) continue;

      kprintf("FS: FOUND on port %u lba=%llu\n", port, p.lba_start);

      uint8_t bs[512];
      int rrc = disk_read(&d, p.lba_start, 1, bs);
      kprintf("FS: read bs rc=%d sig=%02x%02x at port=%u lba=%llu\n",
              rrc, bs[510], bs[511], port, (unsigned long long)p.lba_start);

      out->disk = d;
      out->port = port;
      out->root_part = p;

      rc = fat16_mount(&out->fat, &out->disk, p.lba_start);
      kprintf("FS: fat16_mount rc=%d\n", rc);
      return rc;
    }

    kprintf("FS: root partuuid not found: %s\n", uuid);
    return -20;
  }

  kprintf("FS: unsupported root_spec: %s\n", rs);
  return -21;
}

// fs_read_file: alloc+read whole file into kmalloc buffer.
// Caller must kfree(*out_buf).
int fs_read_file(Fs *fs, const char *path, void **out_buf, uint32_t *out_size)
{
  if (out_buf)  *out_buf  = 0;
  if (out_size) *out_size = 0;
  if (!fs || !path || !out_buf || !out_size) return -1;

  char p83[256];
  norm_path83(path, p83, sizeof(p83));

  uint16_t clus = 0;
  uint8_t  attr = 0;
  uint32_t size = 0;

  int rc = fat16_stat_path83(&fs->fat, p83, &clus, &attr, &size);
  if (rc != 0) return rc;

  if (attr & FAT_ATTR_DIR) return -2;

  if (size == 0) {
    *out_buf = 0;
    *out_size = 0;
    return 0;
  }

  void *buf = kmalloc(size);
  if (!buf) return -3;

  rc = fat16_read_file_by_clus(&fs->fat, clus, 0, size, buf);
  if (rc != 0) {
    kfree(buf);
    return rc;
  }

  *out_buf = buf;
  *out_size = size;
  return 0;
}

// fs_read_file_at: read into caller buffer (no alloc).
int fs_read_file_at(Fs *fs, const char *path,
                    uint32_t offset, void *buf, uint32_t len,
                    uint32_t *out_read)
{
  if (out_read) *out_read = 0;
  if (!fs || !path || !buf) return -1;
  if (len == 0) return 0;

  char p83[256];
  norm_path83(path, p83, sizeof(p83));

  uint16_t clus = 0;
  uint8_t  attr = 0;
  uint32_t size = 0;

  int rc = fat16_stat_path83(&fs->fat, p83, &clus, &attr, &size);
  if (rc != 0) return rc;

  if (attr & FAT_ATTR_DIR) return -2;
  if (offset >= size) return -4; // EOF

  uint32_t remain = size - offset;
  uint32_t take = (len < remain) ? len : remain;

  rc = fat16_read_file_by_clus(&fs->fat, clus, offset, take, buf);
  if (rc != 0) return rc;

  if (out_read) *out_read = take;
  return 0;
}

int fs_mkdir(Fs *fs, const char *path)
{
  if (!fs || !path) return -1;

  char p83[256];
  norm_path83(path, p83, sizeof(p83));

  // write API is only visible via fat16_w.h
  return fat16_mkdir_path83(&fs->fat, p83);
}

int fs_listdir(Fs *fs, const char *path, fs_listdir_cb cb, void *ud)
{
  if (!fs || !cb) return -1;

  FatDirIter it;
  int rc = 0;

  if (is_root_path(path)) {
    rc = fat16_root_iter_begin(&fs->fat, &it);
    if (rc != 0) return rc;
  } else {
    char p83[256];
    norm_path83(path, p83, sizeof(p83));

    uint8_t  attr = 0;
    uint16_t clus = 0;
    uint32_t size = 0;

    rc = fat16_stat_path83(&fs->fat, p83, &clus, &attr, &size);
    if (rc != 0) return rc;

    if ((attr & FAT_ATTR_DIR) == 0) return -2;

    rc = fat16_dir_iter_begin(&fs->fat, &it, clus);
    if (rc != 0) return rc;
  }

  int n = 0;
  for (;;) {
    char name83[13];
    uint8_t attr = 0;
    uint16_t clus = 0;
    uint32_t size = 0;

    rc = fat16_dir_iter_next(&it, name83, &attr, &clus, &size);
    if (rc > 0) break;      // end
    if (rc < 0) return rc;  // error

    // skip "." and ".."
    if (name83[0] == '.' && name83[1] == 0) continue;
    if (name83[0] == '.' && name83[1] == '.' && name83[2] == 0) continue;

    int cb_rc = cb(ud, name83, attr, size);
    if (cb_rc != 0) break;

    n++;
  }

  return n;
}

int fs_stat(Fs *fs, const char *path, FsStat *st)
{
  if (st) *st = (FsStat){0};
  if (!fs || !path || !st) return -1;

  char p83[256];
  norm_path83(path, p83, sizeof(p83));

  uint16_t clus = 0;
  uint8_t  attr = 0;
  uint32_t size = 0;

  int rc = fat16_stat_path83(&fs->fat, p83, &clus, &attr, &size);
  if (rc != 0) return rc;

  st->size = size;
  st->type = (attr & FAT_ATTR_DIR) ? FS_TYPE_DIR : FS_TYPE_FILE;
  return 0;
}