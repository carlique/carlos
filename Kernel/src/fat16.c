// fat16.c
#include <stdint.h>
#include <stddef.h>
#include <carlos/fat16.h>
#include <carlos/klog.h>

// fat16.c logging (runtime controlled by g_klog_level + g_klog_mask)
#define FAT_TRACE(...) KLOG(KLOG_MOD_FAT, KLOG_TRACE, __VA_ARGS__)
#define FAT_DBG(...)   KLOG(KLOG_MOD_FAT, KLOG_DBG,   __VA_ARGS__)
#define FAT_INFO(...)  KLOG(KLOG_MOD_FAT, KLOG_INFO,   __VA_ARGS__)
#define FAT_WARN(...)  KLOG(KLOG_MOD_FAT, KLOG_WARN,  __VA_ARGS__)
#define FAT_ERR(...)   KLOG(KLOG_MOD_FAT, KLOG_ERR,   __VA_ARGS__)

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
  if (rc != 0) {
    FAT_ERR("fat: next_clus read fat0 clus=%u sec=%u rc=%d\n",
            (unsigned)clus, (unsigned)sec, rc);
    return -2;
  }

  uint16_t v0 = rd16(&buf[idx]);

  if (v0 == 0 && fs->nfats > 1) {
    uint64_t fat1_lba = fs->fat_lba + (uint64_t)fs->fatsz;
    rc = fat_read_sector(fs, fat1_lba + sec, buf);
    if (rc == 0) {
      uint16_t v1 = rd16(&buf[idx]);
      if (v1 != 0) {
        FAT_WARN("fat: next_clus fat0=0 fat1=%u clus=%u\n",
                 (unsigned)v1, (unsigned)clus);
        *out = v1;
        return 0;
      }
    } else {
      FAT_WARN("fat: next_clus read fat1 failed clus=%u rc=%d\n",
               (unsigned)clus, rc);
    }
  }

  *out = v0;
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
  if (rc != 0) { FAT_ERR("fat: mount read bs lba=%llu rc=%d\n",
                         (unsigned long long)base_lba, rc); return rc; }

  if (bs[510] != 0x55 || bs[511] != 0xAA) {
    FAT_ERR("fat: bad bs sig %02x%02x lba=%llu\n", bs[510], bs[511],
            (unsigned long long)base_lba);
    return -2;
  }

  fs->bps      = rd16(&bs[11]);
  fs->spc      = bs[13];
  fs->rsvd     = rd16(&bs[14]);
  fs->nfats    = bs[16];
  fs->root_ent = rd16(&bs[17]);
  fs->fatsz    = rd16(&bs[22]);

  if (fs->bps != 512) { FAT_ERR("fat: bps=%u (want 512)\n", (unsigned)fs->bps); return -3; }
  if (fs->spc == 0)   { FAT_ERR("fat: spc=0\n"); return -4; }
  if (fs->nfats == 0) { FAT_ERR("fat: nfats=0\n"); return -5; }
  if (fs->fatsz == 0) { FAT_ERR("fat: fatsz=0\n"); return -6; }

  fs->fat_lba = base_lba + (uint64_t)fs->rsvd;
  fs->root_secs = (uint32_t)(((uint32_t)fs->root_ent * 32u + (fs->bps - 1)) / fs->bps);
  fs->root_lba  = fs->fat_lba + (uint64_t)fs->nfats * (uint64_t)fs->fatsz;
  fs->data_lba  = fs->root_lba + (uint64_t)fs->root_secs;

  FAT_INFO("fat: mount ok base=%llu bps=%u spc=%u rsvd=%u nfats=%u root_ent=%u fatsz=%u\n",
           (unsigned long long)base_lba,
           (unsigned)fs->bps, (unsigned)fs->spc, (unsigned)fs->rsvd,
           (unsigned)fs->nfats, (unsigned)fs->root_ent, (unsigned)fs->fatsz);
  FAT_DBG("fat: lbas fat=%llu root=%llu data=%llu root_secs=%u\n",
          (unsigned long long)fs->fat_lba,
          (unsigned long long)fs->root_lba,
          (unsigned long long)fs->data_lba,
          (unsigned)fs->root_secs);

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
    if (rc != 0) {
      if (rc == 1) FAT_DBG("fat: find_in_dir miss '%s'\n", tname);
      return rc;
    }

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
  FAT_TRACE("fat: stat '%s'\n", path);

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
    if (rc != 0) return rc; // propagate: 1=end/not found, <0=io/format errors

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

int fat16_read_file_by_clus(Fat16 *fs, uint16_t first_clus,
                            uint32_t offset, uint32_t size, void *out)
{
  FAT_DBG("fat: read clus=%u off=%u size=%u\n",
        (unsigned)first_clus, (unsigned)offset, (unsigned)size);

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
    if (clus_is_eoc(nxt)) { FAT_ERR("fat: read hit eoc clus=%u\n", (unsigned)clus); return -7; }
    if (nxt < 2)          { FAT_ERR("fat: read bad next=%u from clus=%u\n", (unsigned)nxt, (unsigned)clus); return -8; }
  }
  return 0;
}

static int fat_write_sector(Fat16 *fs, uint64_t lba, const void *buf){
  if (!fs || !fs->disk) return -1;
  if (fs->disk->sector_size != 512) return -2;
  return disk_write(fs->disk, lba, 1, buf);
}

static int fat16_set_fat_entry(Fat16 *fs, uint16_t clus, uint16_t val){
  // FAT16 entry is 2 bytes at offset clus*2
  uint32_t off = (uint32_t)clus * 2u;
  uint32_t sec = off / fs->bps;
  uint32_t idx = off % fs->bps;

  uint8_t buf[512];

  for (uint8_t fi = 0; fi < fs->nfats; fi++){
    uint64_t fat_base = fs->fat_lba + (uint64_t)fi * (uint64_t)fs->fatsz;

    int rc = disk_read(fs->disk, fat_base + sec, 1, buf);
    if (rc != 0) return rc;

    buf[idx + 0] = (uint8_t)(val & 0xFF);
    buf[idx + 1] = (uint8_t)((val >> 8) & 0xFF);

    rc = fat_write_sector(fs, fat_base + sec, buf);
    if (rc != 0) return rc;
  }

  return 0;
}

int fat16_alloc_clus(Fat16 *fs, uint16_t *out_clus){
  if (!fs || !out_clus) return -1;

  // total FAT16 entries ~= fatsz*bps/2
  uint32_t max_entries = (uint32_t)fs->fatsz * (uint32_t)fs->bps / 2u;
  if (max_entries < 3) return -2;

  uint8_t secbuf[512];

  // start scanning from cluster 2
  uint32_t clus = 2;
  uint32_t fat_off = clus * 2u;

  for (uint32_t sec = fat_off / fs->bps; sec < fs->fatsz; sec++){
    int rc = disk_read(fs->disk, fs->fat_lba + sec, 1, secbuf);
    if (rc != 0) return rc;

    uint32_t start_idx = 0;
    if (sec == fat_off / fs->bps) start_idx = fat_off % fs->bps;

    for (uint32_t i = start_idx; i + 1 < fs->bps; i += 2){
      uint16_t v = (uint16_t)secbuf[i] | ((uint16_t)secbuf[i+1] << 8);
      if (v == 0x0000){
        uint16_t found = (uint16_t)((sec * (fs->bps/2u)) + (i/2u));
        if (found < 2) continue;

        // mark EOC
        rc = fat16_set_fat_entry(fs, found, 0xFFFF);
        if (rc != 0) return rc;

        *out_clus = found;
        FAT_INFO("fat: alloc clus=%u\n", (unsigned)found);
        return 0;
      }
    }
  }

  return -3; // no free cluster
}

static int split_parent_leaf(const char *path, char *parent, uint32_t parent_cap, char *leaf, uint32_t leaf_cap){
  if (!path || !parent || !leaf || parent_cap==0 || leaf_cap==0) return -1;

  // skip leading seps
  while (*path && is_sep(*path)) path++;

  // find last sep
  const char *last = 0;
  for (const char *p = path; *p; p++){
    if (is_sep(*p)) last = p;
  }

  if (!last){
    // parent = root
    parent[0] = 0;
    // leaf = whole path
    uint32_t n = 0;
    while (path[n] && n + 1 < leaf_cap){ leaf[n] = path[n]; n++; }
    leaf[n] = 0;
    return (leaf[0] ? 0 : -2);
  }

  // parent = [path..last)
  uint32_t pn = 0;
  for (const char *p = path; p < last && pn + 1 < parent_cap; p++) parent[pn++] = *p;
  parent[pn] = 0;

  // leaf = after last sep
  const char *l = last + 1;
  uint32_t ln = 0;
  while (l[ln] && ln + 1 < leaf_cap){ leaf[ln] = l[ln]; ln++; }
  leaf[ln] = 0;

  return (leaf[0] ? 0 : -3);
}

static void dirent_clear(FatDirEnt *e){
  __builtin_memset(e, 0, sizeof(*e));
}

static int write_dirent_into_sector(Fat16 *fs, uint64_t lba, uint32_t ent_index, const FatDirEnt *src){
  uint8_t sec[512];
  int rc = disk_read(fs->disk, lba, 1, sec);
  if (rc != 0) return rc;

  FatDirEnt *dst = (FatDirEnt*)(void*)(sec + ent_index * 32u);
  __builtin_memcpy(dst, src, sizeof(*dst));

  return fat_write_sector(fs, lba, sec);
}

static int find_free_dirent_root(Fat16 *fs, uint64_t *out_lba, uint32_t *out_ent){
  uint8_t sec[512];

  uint32_t ents_per_sec = fs->bps / 32u;
  uint32_t total_ents = fs->root_ent;

  for (uint32_t e = 0; e < total_ents; e++){
    uint32_t sec_idx = e / ents_per_sec;
    uint32_t ent_idx = e % ents_per_sec;
    uint64_t lba = fs->root_lba + sec_idx;

    int rc = disk_read(fs->disk, lba, 1, sec);
    if (rc != 0) return rc;

    const FatDirEnt *de = (const FatDirEnt*)(const void*)(sec + ent_idx * 32u);
    uint8_t first = de->Name[0];

    if (first == 0x00 || first == 0xE5){
      *out_lba = lba;
      *out_ent = ent_idx;
      return 0;
    }
  }

  return -10; // root full
}

static int find_free_dirent_cluschain(Fat16 *fs, uint16_t first_clus,
                                      uint16_t *io_last_clus, uint64_t *out_lba, uint32_t *out_ent)
{
  uint8_t sec[512];
  uint16_t clus = first_clus;

  for (;;) {
    if (clus < 2) return -11;

    for (uint32_t s = 0; s < fs->spc; s++){
      uint64_t lba = clus_to_lba(fs, clus) + s;

      int rc = disk_read(fs->disk, lba, 1, sec);
      if (rc != 0) return rc;

      uint32_t ents = fs->bps / 32u;
      for (uint32_t i = 0; i < ents; i++){
        const FatDirEnt *de = (const FatDirEnt*)(const void*)(sec + i*32u);
        uint8_t first = de->Name[0];
        if (first == 0x00 || first == 0xE5){
          *io_last_clus = clus;
          *out_lba = lba;
          *out_ent = i;
          return 0;
        }
      }
    }

    uint16_t nxt = 0;
    int frc = fat_next_clus(fs, clus, &nxt);
    if (frc != 0) return frc;   // or: return -something, if you prefer
    if (clus_is_eoc(nxt)){
      *io_last_clus = clus;
      return 1; // no free slot, at end-of-chain
    }
    if (nxt < 2) return -12;
    clus = nxt;
  }
}

static int init_dir_cluster(Fat16 *fs, uint16_t new_clus, uint16_t parent_clus_or_0){
  uint8_t sec[512];
  __builtin_memset(sec, 0, sizeof(sec));

  FatDirEnt dot, dotdot;
  dirent_clear(&dot);
  dirent_clear(&dotdot);

  // "." name
  for (int i=0;i<11;i++) dot.Name[i] = ' ';
  dot.Name[0] = '.';
  dot.Attr = ATTR_DIR;
  dot.FstClusLO = new_clus;

  // ".." name
  for (int i=0;i<11;i++) dotdot.Name[i] = ' ';
  dotdot.Name[0] = '.';
  dotdot.Name[1] = '.';
  dotdot.Attr = ATTR_DIR;
  dotdot.FstClusLO = parent_clus_or_0; // 0 if parent is root

  __builtin_memcpy(sec + 0*32u, &dot, sizeof(dot));
  __builtin_memcpy(sec + 1*32u, &dotdot, sizeof(dotdot));

  uint64_t first_lba = clus_to_lba(fs, new_clus);
  int rc = fat_write_sector(fs, first_lba, sec);
  if (rc != 0) return rc;

  // zero remaining sectors in the cluster (if spc>1)
  __builtin_memset(sec, 0, sizeof(sec));
  for (uint32_t s = 1; s < fs->spc; s++){
    rc = fat_write_sector(fs, first_lba + s, sec);
    if (rc != 0) return rc;
  }

  return 0;
}

int fat16_mkdir_path83(Fat16 *fs, const char *path83){
  FAT_INFO("fat: mkdir '%s'\n", path83);

  if (!fs || !path83) return -1;

  // If already exists -> error
  {
    uint16_t c; uint8_t a; uint32_t sz;
    if (fat16_stat_path83(fs, path83, &c, &a, &sz) == 0) return -2;
  }

  char parent[128];
  char leaf[64];
  int rc = split_parent_leaf(path83, parent, sizeof(parent), leaf, sizeof(leaf));
  if (rc != 0) return -3;

  // Resolve parent directory
  int parent_is_root = (parent[0] == 0);
  uint16_t parent_clus = 0;

  if (!parent_is_root){
    uint16_t c; uint8_t a; uint32_t sz;
    rc = fat16_stat_path83(fs, parent, &c, &a, &sz);
    if (rc != 0) return -4;
    if ((a & ATTR_DIR) == 0) return -5;
    parent_clus = c;
  }

  // Create new directory cluster
  uint16_t new_clus = 0;
  rc = fat16_alloc_clus(fs, &new_clus);
  if (rc != 0) return rc;

  // Init "." and ".."
  rc = init_dir_cluster(fs, new_clus, parent_is_root ? 0 : parent_clus);
  if (rc != 0) return rc;

  // Build parent dir entry
  uint8_t name11[11];
  rc = make_name11(leaf, name11);
  if (rc != 0) return -6;

  FatDirEnt ent;
  dirent_clear(&ent);
  __builtin_memcpy(ent.Name, name11, 11);
  ent.Attr = ATTR_DIR;
  ent.FstClusLO = new_clus;
  ent.FileSize = 0;

  // Insert into parent directory
  if (parent_is_root){
    uint64_t lba; uint32_t ei;
    rc = find_free_dirent_root(fs, &lba, &ei);
    if (rc != 0) return rc;
    return write_dirent_into_sector(fs, lba, ei, &ent);
  } else {
    uint16_t last = parent_clus;
    uint64_t lba; uint32_t ei;
    rc = find_free_dirent_cluschain(fs, parent_clus, &last, &lba, &ei);
    if (rc == 0){
      FAT_INFO("fat: mkdir ok '%s' clus=%u\n", path83, (unsigned)new_clus);
      return write_dirent_into_sector(fs, lba, ei, &ent);
    }
    if (rc < 0) return rc;

    // rc==1 => need to extend chain
    uint16_t ext = 0;
    rc = fat16_alloc_clus(fs, &ext);
    if (rc != 0) return rc;

    // link last -> ext
    rc = fat16_set_fat_entry(fs, last, ext);
    if (rc != 0) return rc;

    // ext is already EOC (alloc did that); clear ext cluster and use first entry
    uint8_t zero[512];
    __builtin_memset(zero, 0, sizeof(zero));
    uint64_t base = clus_to_lba(fs, ext);
    for (uint32_t s=0; s<fs->spc; s++){
      rc = fat_write_sector(fs, base + s, zero);
      if (rc != 0) return rc;
    }

    // write entry into first slot of ext
    FAT_INFO("fat: mkdir ok '%s' clus=%u\n", path83, (unsigned)new_clus);
    return write_dirent_into_sector(fs, base, 0, &ent);
  }
}