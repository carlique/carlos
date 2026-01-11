#include <carlos/kapi.h>
#include <carlos/klog.h>
#include <carlos/pmm.h>
#include <carlos/fat16.h>
#include <carlos/str.h>
#include <carlos/path.h>

// kapi.c
#define KAPI_FS_DEBUG 1

#if KAPI_FS_DEBUG
  #define KAPI_DBG(...) kprintf(__VA_ARGS__)
#else
  #define KAPI_DBG(...) do{}while(0)
#endif

static Fs *g_kapi_fs = 0;

static char g_kapi_cwd[128] = "/";   // POSIX-ish, "/" or "/BIN"

extern void carlos_kexit(int code); // from exec_s.S

void kapi_bind_fs(Fs *fs){
  g_kapi_fs = fs;
}

static s32 api_write(const char *buf, size_t len){
  for (size_t i = 0; i < len; i++) kputc(buf[i]);
  return (s32)len;
}

void kapi_set_cwd(const char *cwd){
  if (!cwd || cwd[0] == 0) { g_kapi_cwd[0] = '/'; g_kapi_cwd[1] = 0; return; }

  // copy (no libc)
  size_t i = 0;
  for (; i + 1 < sizeof(g_kapi_cwd) && cwd[i]; i++) g_kapi_cwd[i] = cwd[i];
  g_kapi_cwd[i] = 0;

  // normalize: must start with '/'
  if (g_kapi_cwd[0] != '/') {
    // shift right by 1 if room
    if (i + 2 < sizeof(g_kapi_cwd)) {
      for (size_t j = i + 1; j > 0; j--) g_kapi_cwd[j] = g_kapi_cwd[j - 1];
      g_kapi_cwd[0] = '/';
    } else {
      g_kapi_cwd[0] = '/'; g_kapi_cwd[1] = 0;
    }
  }
}

static void* api_alloc_pages(size_t pages){
  if (pages != 1) return 0;
  return pmm_alloc_page();
}

static void api_free_pages(void *p, size_t pages){
  if (!p || pages != 1) return;
  pmm_free_page(p);
}

static void normalize_abs_path(char *p){
  // In-place normalize for absolute POSIX-ish paths:
  // - convert '\' to '/'
  // - collapse //, resolve /./ and /../
  // - keep leading '/'
  // - drop trailing '/' except root
  if (!p) return;

  char out[256];
  size_t oi = 0;

  // ensure leading '/'
  out[oi++] = '/';
  out[oi]   = 0;   // <-- IMPORTANT: makes "/" a valid C string

  // read cursor (skip leading seps in input)
  size_t i = 0;
  while (p[i] && path_is_sep(p[i])) i++;

  while (p[i]) {
    // skip repeated seps
    while (p[i] && path_is_sep(p[i])) i++;
    if (!p[i]) break;

    // read one component
    char comp[64];
    size_t ci = 0;
    while (p[i] && !path_is_sep(p[i]) && ci + 1 < sizeof(comp)) {
      char c = p[i++];
      comp[ci++] = (c == '\\') ? '/' : c;
    }
    comp[ci] = 0;

    if (ci == 0) break;

    // "." => skip
    if (ci == 1 && comp[0] == '.') continue;

    // ".." => pop
    if (ci == 2 && comp[0] == '.' && comp[1] == '.') {
      if (oi > 1) {
        // remove trailing slash if present
        if (oi > 1 && out[oi-1] == '/') oi--;
        // rewind to previous slash
        while (oi > 1 && out[oi-1] != '/') oi--;
      }
      continue;
    }

    // append "/comp"
    if (oi > 1 && out[oi-1] != '/' && oi + 1 < sizeof(out)) out[oi++] = '/';
    for (size_t k = 0; comp[k] && oi + 1 < sizeof(out); k++) out[oi++] = comp[k];
    out[oi] = 0;
  }

  // trim trailing slash except root
  if (oi > 1 && out[oi-1] == '/') { out[--oi] = 0; }

  // copy back
  for (size_t k = 0; k + 1 < sizeof(out); k++) {
    p[k] = out[k];
    if (!out[k]) break;
  }
  p[sizeof(out) - 1] = 0;
}

static void dbg_show_path(const char *tag, const char *s){
  if (!s) { KAPI_DBG("kapi: %s = (null)\n", tag); return; }
  KAPI_DBG("kapi: %s = '%s'\n", tag, s);
}

static void dbg_show_path_bytes(const char *tag, const char *s){
  KAPI_DBG("kapi: %s bytes:", tag);
  if (!s) { KAPI_DBG(" (null)\n"); return; }
  for (int i = 0; i < 40 && s[i]; i++){
    unsigned char c = (unsigned char)s[i];
    KAPI_DBG(" %02x", (unsigned)c);
  }
  KAPI_DBG("\n");
}

static void dbg_show_iter_rc(int rc){
  if (rc > 0) KAPI_DBG("kapi: iter_next => END rc=%d\n", rc);
  else if (rc < 0) KAPI_DBG("kapi: iter_next => ERR rc=%d\n", rc);
}

static s32 api_fs_listdir(const char *path, CarlosDirEnt *ents, u32 max_ents){
  if (!g_kapi_fs || !ents || max_ents == 0) return -1;

  KAPI_DBG("\n--- kapi: fs_listdir ---\n");
  dbg_show_path("cwd", g_kapi_cwd);
  dbg_show_path("arg", path);
  dbg_show_path_bytes("arg", path);

  // NULL, "" or "." => cwd
  const char *p = path;
  if (!p || p[0] == 0 || (p[0] == '.' && p[1] == 0)) p = g_kapi_cwd;
  dbg_show_path("p(after default)", p);

  // Build absolute POSIX-ish path into abs[]
  char abs[512];

  const int is_abs = (p[0] == '/' || p[0] == '\\');
  KAPI_DBG("kapi: is_abs=%d\n", is_abs);

  if (is_abs) {
    kstrncpy(abs, p, sizeof(abs)-1);
    abs[sizeof(abs)-1] = 0;
  } else {
    const char *cwd = (g_kapi_cwd[0] ? g_kapi_cwd : "/");
    size_t a = kstrlen(cwd);
    size_t b = kstrlen(p);

    if (a == 1 && cwd[0] == '/') a = 0;

    KAPI_DBG("kapi: join: cwd='%s' (a=%u) + p='%s' (b=%u)\n",
             cwd, (unsigned)a, p, (unsigned)b);

    if (1 + a + (a ? 1 : 0) + b + 1 > sizeof(abs)) {
      KAPI_DBG("kapi: join overflow -> -2\n");
      return -2;
    }

    size_t j = 0;
    abs[j++] = '/';

    for (size_t i = 0; i < a; i++) {
      char c = cwd[i];
      if (c == '\\') c = '/';
      abs[j++] = c;
    }
    if (a) abs[j++] = '/';

    for (size_t i = 0; i < b; i++) {
      char c = p[i];
      if (c == '\\') c = '/';
      abs[j++] = c;
    }
    abs[j] = 0;
  }

  dbg_show_path("abs(before norm)", abs);

  // normalize (handles ../ and .)
  normalize_abs_path(abs);

  dbg_show_path("abs(after norm)", abs);

  // fat16_* wants root-relative without leading slash.
  const char *fatp = abs;
  while (fatp[0] == '/' || fatp[0] == '\\') fatp++;

  dbg_show_path("fatp", fatp);
  dbg_show_path_bytes("fatp", fatp);

  FatDirIter it;
  int rc;

  if (!fatp[0]) {
    KAPI_DBG("kapi: iter root\n");
    rc = fat16_root_iter_begin(&g_kapi_fs->fat, &it);
    KAPI_DBG("kapi: root_iter_begin rc=%d\n", rc);
    if (rc != 0) return rc;
  } else {
    uint16_t clus = 0;
    uint8_t  attr = 0;
    uint32_t size = 0;

    KAPI_DBG("kapi: stat '%s'\n", fatp);
    rc = fat16_stat_path83(&g_kapi_fs->fat, fatp, &clus, &attr, &size);
    KAPI_DBG("kapi: stat rc=%d clus=%u attr=0x%02x size=%u\n",
             rc, (unsigned)clus, (unsigned)attr, (unsigned)size);
    if (rc != 0) return rc;

    if ((attr & 0x10) == 0) {
      KAPI_DBG("kapi: not a dir -> -3\n");
      return -3;
    }

    KAPI_DBG("kapi: dir_iter_begin clus=%u\n", (unsigned)clus);
    rc = fat16_dir_iter_begin(&g_kapi_fs->fat, &it, clus);
    KAPI_DBG("kapi: dir_iter_begin rc=%d\n", rc);
    if (rc != 0) return rc;
  }

  u32 n = 0;
  while (n < max_ents) {
    char name83[13];
    uint8_t attr = 0;
    uint16_t clus = 0;
    uint32_t size = 0;

    rc = fat16_dir_iter_next(&it, name83, &attr, &clus, &size);
    dbg_show_iter_rc(rc);
    if (rc > 0) break;
    if (rc < 0) return rc;

    KAPI_DBG("kapi: ent name83='%s' attr=0x%02x clus=%u size=%u\n",
             name83, (unsigned)attr, (unsigned)clus, (unsigned)size);

    if (name83[0] == '.' && name83[1] == 0) continue;
    if (name83[0] == '.' && name83[1] == '.' && name83[2] == 0) continue;

    for (int i = 0; i < CARLOS_NAME_MAX; i++) ents[n].name[i] = 0;
    for (int i = 0; i < (CARLOS_NAME_MAX - 1) && name83[i]; i++) ents[n].name[i] = name83[i];

    ents[n].size = size;
    ents[n].type = (attr & 0x10) ? 2 : 1;
    n++;
  }

  KAPI_DBG("kapi: fs_listdir => n=%u\n", (unsigned)n);
  return (s32)n;
}

const CarlosApi g_api = {
  .abi_version = CARLOS_ABI_VERSION,
  .write       = api_write,
  .exit        = carlos_kexit,
  .alloc_pages = api_alloc_pages,
  .free_pages  = api_free_pages,
  .fs_listdir  = api_fs_listdir,
};