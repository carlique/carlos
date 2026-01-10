#include <stdint.h>
#include <stddef.h>
#include <carlos/fat16.h>

// ---------- tiny helpers (no libc needed) ----------
static inline uint16_t rd16(const void *p){
  const uint8_t *b = (const uint8_t*)p;
  return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}
/*
static inline uint32_t rd32(const void *p){
  const uint8_t *b = (const uint8_t*)p;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
*/

static inline void memclr(void *p, size_t n){ __builtin_memset(p, 0, n); }
static inline void memcp(void *d, const void *s, size_t n){ __builtin_memcpy(d, s, n); }
//static inline int memeq(const void *a, const void *b, size_t n){ return __builtin_memcmp(a,b,n) == 0; }

static inline int is_sep(char c){ return c == '/' || c == '\\'; }
static inline char upc(char c){ return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c; }

// FAT dir entry
typedef struct __attribute__((packed)) {
  uint8_t  Name[11];
  uint8_t  Attr;
  uint8_t  NtRes;
  uint8_t  CrtTimeTenth;
  uint16_t CrtTime;
  uint16_t CrtDate;
  uint16_t LstAccDate;
  uint16_t FstClusHI;
  uint16_t WrtTime;
  uint16_t WrtDate;
  uint16_t FstClusLO;
  uint32_t FileSize;
} FatDirEnt;

#define ATTR_LFN  0x0F
#define ATTR_VOL  0x08
#define ATTR_DIR  0x10

static uint64_t clus_to_lba(const Fat16 *fs, uint16_t clus){
  // cluster numbers start at 2
  return fs->data_lba + (uint64_t)(clus - 2) * (uint64_t)fs->spc;
}

static int fat_read_sector(Fat16 *fs, uint64_t lba, void *buf){
  if (!fs || !fs->disk) return -1;
  if (fs->disk->sector_size != 512) return -2;
  return disk_read(fs->disk, lba, 1, buf);
}

static int fat_next_clus(Fat16 *fs, uint16_t clus, uint16_t *out){
  if (!fs || !out) return -1;

  uint32_t off = (uint32_t)clus * 2u;
  uint32_t sec = off / fs->bps;
  uint32_t idx = off % fs->bps;

  uint8_t buf[512];
  int rc = fat_read_sector(fs, fs->fat_lba + sec, buf);
  if (rc != 0) return -2;

  *out = rd16(&buf[idx]);
  return 0;
}
static int clus_is_eoc(uint16_t clus){
  return clus >= 0xFFF8u;
}

// Build FAT 8.3 uppercase padded 11-byte name from component ("KERNEL.ELF")
static int make_name11(const char *comp, uint8_t out11[11]){
  if (!comp || !out11) return -1;
  for (int i=0;i<11;i++) out11[i] = ' ';

  int i = 0;
  int n = 0;

  // name (up to 8)
  while (comp[i] && comp[i] != '.' && !is_sep(comp[i])) {
    char c = upc(comp[i]);
    if (n >= 8) return -2;
    out11[n++] = (uint8_t)c;
    i++;
  }

  // ext
  if (comp[i] == '.') {
    i++;
    int e = 0;
    while (comp[i] && !is_sep(comp[i])) {
      char c = upc(comp[i]);
      if (e >= 3) return -3;
      out11[8 + e] = (uint8_t)c;
      e++;
      i++;
    }
  }

  // must end at separator or NUL
  while (comp[i] && !is_sep(comp[i])) return -4;

  return 0;
}

static void name11_to_name83(const uint8_t n11[11], char out[13]){
  // out "NAME.EXT" or "NAME"
  int p = 0;

  // name part
  for (int i=0;i<8;i++){
    char c = (char)n11[i];
    if (c == ' ') break;
    if (p < 12) out[p++] = c;
  }

  // ext part?
  int has_ext = 0;
  for (int i=8;i<11;i++){
    if (n11[i] != ' ') { has_ext = 1; break; }
  }

  if (has_ext && p < 12) out[p++] = '.';

  for (int i=8;i<11;i++){
    char c = (char)n11[i];
    if (c == ' ') break;
    if (p < 12) out[p++] = c;
  }

  out[p] = 0;
}

static int iter_load_sector(FatDirIter *it, uint64_t lba){
  if (it->cur_lba == lba) return 0;
  int rc = fat_read_sector(it->fs, lba, it->secbuf);
  if (rc != 0) return rc;
  it->cur_lba = lba;
  it->ent_idx = 0;
  return 0;
}

static int dir_iter_begin_root(Fat16 *fs, FatDirIter *it){
  memclr(it, sizeof(*it));
  it->fs = fs;
  it->in_root = 1;
  it->done = 0;
  it->root_entries_left = fs->root_ent;
  it->cur_lba = (uint64_t)(~0ull);
  return 0;
}

static int dir_iter_begin_clus(Fat16 *fs, FatDirIter *it, uint16_t first_clus){
  memclr(it, sizeof(*it));
  it->fs = fs;
  it->in_root = 0;
  it->done = 0;
  it->cur_clus = first_clus;
  it->sec_in_clus = 0;
  it->cur_lba = (uint64_t)(~0ull);
  return 0;
}

int fat16_dir_iter_begin(Fat16 *fs, FatDirIter *it, uint16_t first_clus){
  if (!fs || !it) return -1;
  if (first_clus < 2) return -2;
  return dir_iter_begin_clus(fs, it, first_clus);
}

int fat16_mount(Fat16 *fs, Disk *disk, uint64_t base_lba){
  if (!fs || !disk) return -1;
  memclr(fs, sizeof(*fs));

  fs->disk = disk;
  fs->base_lba = base_lba;

  uint8_t bs[512];
  int rc = disk_read(disk, base_lba, 1, bs);
  if (rc != 0) return rc;

  // signature 0x55AA at end of boot sector
  if (bs[510] != 0x55 || bs[511] != 0xAA) return -2;

  fs->bps      = rd16(&bs[11]);   // BPB_BytsPerSec
  fs->spc      = bs[13];          // BPB_SecPerClus
  fs->rsvd     = rd16(&bs[14]);   // BPB_RsvdSecCnt
  fs->nfats    = bs[16];          // BPB_NumFATs
  fs->root_ent = rd16(&bs[17]);   // BPB_RootEntCnt
  fs->fatsz    = rd16(&bs[22]);   // BPB_FATSz16

  if (fs->bps != 512) return -3;
  if (fs->spc == 0) return -4;
  if (fs->nfats == 0) return -5;
  if (fs->fatsz == 0) return -6;

  fs->fat_lba = base_lba + (uint64_t)fs->rsvd;

  fs->root_secs = (uint32_t)(((uint32_t)fs->root_ent * 32u + (fs->bps - 1)) / fs->bps);
  fs->root_lba  = fs->fat_lba + (uint64_t)fs->nfats * (uint64_t)fs->fatsz;

  fs->data_lba  = fs->root_lba + (uint64_t)fs->root_secs;

  return 0;
}

int fat16_root_iter_begin(Fat16 *fs, FatDirIter *it){
  if (!fs || !it) return -1;
  return dir_iter_begin_root(fs, it);
}

int fat16_dir_iter_next(FatDirIter *it, char name83[13],
                        uint8_t *attr, uint16_t *clus, uint32_t *size)
{
  if (!it || !it->fs || it->done) return -1;

  for (;;) {
    // Choose current sector LBA
    uint64_t lba = 0;

    if (it->in_root) {
      if (it->root_entries_left == 0) { it->done = 1; return 1; }
      uint32_t ents_per_sec = it->fs->bps / 32u;
      uint32_t sec_idx = (it->fs->root_ent - it->root_entries_left) / ents_per_sec;
      lba = it->fs->root_lba + sec_idx;
    } else {
      if (it->cur_clus < 2 || clus_is_eoc(it->cur_clus)) { it->done = 1; return 1; }
      lba = clus_to_lba(it->fs, it->cur_clus) + it->sec_in_clus;
    }

    int rc = iter_load_sector(it, lba);
    if (rc != 0) return rc;

    // Process entries in this sector
    while (it->ent_idx < (it->fs->bps / 32u)) {
      const FatDirEnt *e = (const FatDirEnt*)(const void*)(it->secbuf + it->ent_idx * 32u);
      it->ent_idx++;

      if (it->in_root) {
        if (it->root_entries_left) it->root_entries_left--;
      }

      uint8_t first = e->Name[0];

      if (first == 0x00) { it->done = 1; return 1; }      // end of directory
      if (first == 0xE5) continue;                        // deleted
      if (e->Attr == ATTR_LFN) continue;                  // long filename entry
      if (e->Attr & ATTR_VOL) continue;                   // volume label

      if (name83) name11_to_name83(e->Name, name83);
      if (attr) *attr = e->Attr;
      if (clus) *clus = e->FstClusLO;
      if (size) *size = e->FileSize;
      return 0;
    }

    // Sector done -> advance
    if (it->in_root) {
      // next sector implicitly handled via root_entries_left math
      it->cur_lba = (uint64_t)(~0ull);
      it->ent_idx = 0;
      continue;
    } else {
      it->sec_in_clus++;
      if (it->sec_in_clus >= it->fs->spc) {
        it->sec_in_clus = 0;
        uint16_t nxt = 0;
        int rc2 = fat_next_clus(it->fs, it->cur_clus, &nxt);
        if (rc2 != 0) return -20; // error reading FAT

        if (clus_is_eoc(nxt)) { it->done = 1; return 1; }
        if (nxt < 2) return -21;        // invalid/free
        // if (nxt == 0xFFF7) return -22; // bad cluster (optional)
        it->cur_clus = nxt;
      }
      it->cur_lba = (uint64_t)(~0ull);
      it->ent_idx = 0;
      continue;
    }
  }
}

static int find_in_dir(Fat16 *fs, int in_root, uint16_t dir_clus,
                       const uint8_t target11[11],
                       /*out*/ FatDirEnt *out)
{
  FatDirIter it;
  if (in_root) dir_iter_begin_root(fs, &it);
  else         dir_iter_begin_clus(fs, &it, dir_clus);

    // We need raw name[11] for exact match; reload from sector buffer:
    // easiest: recompute from tmp -> target11 is already built, so compare entry name directly:
    // But fat16_dir_iter_next doesn't expose raw name11, so we re-scan by comparing tmp.
    // (Still fine for now, but we’ll do it properly: iterate by re-reading the current entry.)
    // --- Instead: just compare tmp with normalized target (cheap) ---
    // We'll generate a comparable string from target11:
    char tname[13];
    name11_to_name83(target11, tname);

  for (;;) {
    char tmp[13];
    uint8_t a;
    uint16_t c;
    uint32_t s;
    int rc = fat16_dir_iter_next(&it, tmp, &a, &c, &s);
    if (rc != 0) return rc; // 1=end, <0=error

    // case-insensitive compare (both should be uppercase already)
    int ok = 1;
    for (int i=0;i<13;i++){
      char x = tmp[i], y = tname[i];
      if (upc(x) != upc(y)) { ok = 0; break; }
      if (!x && !y) break;
    }
    if (!ok) continue;

    // Found. Fill a synthetic out using what we have.
    if (out) {
      memclr(out, sizeof(*out));
      memcp(out->Name, target11, 11);
      out->Attr = a;
      out->FstClusLO = c;
      out->FileSize = s;
    }
    return 0;
  }
}

int fat16_stat_path83(Fat16 *fs, const char *path,
                      uint16_t *clus, uint8_t *attr, uint32_t *size)
{
  if (!fs || !path) return -1;

  if (clus) *clus = 0;
  if (attr) *attr = 0;
  if (size) *size = 0;

  // Skip leading seps
  while (*path && is_sep(*path)) path++;

  // Empty => root
  if (!*path) {
    if (attr) *attr = ATTR_DIR;
    if (clus) *clus = 0;
    if (size) *size = 0;
    return 0;
  }

  int in_root = 1;
  uint16_t cur_dir_clus = 0;

  while (*path) {
    // Extract one component
    char comp[64];
    int n = 0;
    while (path[n] && !is_sep(path[n])) {
      if (n >= (int)sizeof(comp) - 1) return -2;
      comp[n] = path[n];
      n++;
    }
    comp[n] = 0;

    // Build FAT 11-byte name (upper + padded)
    uint8_t t11[11];
    int rc = make_name11(comp, t11);
    if (rc != 0) return -3;

    // Find in current directory
    FatDirEnt e;
    rc = find_in_dir(fs, in_root, cur_dir_clus, t11, &e);
    if (rc != 0) return -4;

    // Advance path
    path += n;
    while (*path && is_sep(*path)) path++;

    int last = (*path == 0);

    if (!last) {
      if ((e.Attr & ATTR_DIR) == 0) return -5; // tried to traverse through a file
      in_root = 0;
      cur_dir_clus = e.FstClusLO;
      continue;
    }

    if (attr) *attr = e.Attr;
    if (clus) *clus = e.FstClusLO;
    if (size) *size = e.FileSize;
    return 0;
  }

  return -6;
}

int fat16_open_path83(Fat16 *fs, const char *path, uint16_t *clus, uint32_t *size){
  uint8_t attr = 0;
  return fat16_stat_path83(fs, path, clus, &attr, size);
}

int fat16_read_file_by_clus(Fat16 *fs, uint16_t first_clus,
                            uint32_t offset, uint32_t size, void *out)
{
  if (!fs || !out) return -1;
  if (size == 0) return 0;
  if (first_clus < 2) return -2;

  const uint32_t bps = fs->bps;
  const uint32_t spc = fs->spc;
  const uint32_t clus_bytes = bps * spc;

  uint16_t clus = first_clus;

  // skip clusters until we reach offset
  while (offset >= clus_bytes) {
    offset -= clus_bytes;
    uint16_t nxt = 0;
    if (fat_next_clus(fs, clus, &nxt) != 0) return -3;
    if (clus_is_eoc(nxt)) return -4;
    if (nxt < 2) return -5;
        clus = nxt;
    }

  uint8_t secbuf[512];
  uint8_t *dst = (uint8_t*)out;

  while (size > 0) {
    if (clus < 2 || clus_is_eoc(clus)) return -5;

    uint64_t base = clus_to_lba(fs, clus);

    uint32_t sec_index = offset / bps;
    uint32_t sec_off   = offset % bps;

    for (; sec_index < spc && size > 0; sec_index++) {
      int rc = fat_read_sector(fs, base + sec_index, secbuf);
      if (rc != 0) return rc;

      uint32_t take = bps - sec_off;
      if (take > size) take = size;

      memcp(dst, secbuf + sec_off, take);

      dst += take;
      size -= take;

      sec_off = 0;
      offset = 0;
    }

    if (size == 0) break;

    uint16_t nxt = 0;
    if (fat_next_clus(fs, clus, &nxt) != 0) return -6;
    if (clus_is_eoc(nxt)) return -7;
    if (nxt < 2) return -8;
        clus = nxt;
    }

  return 0;
}