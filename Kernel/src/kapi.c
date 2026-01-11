#include <carlos/kapi.h>
#include <carlos/klog.h>
#include <carlos/pmm.h>
#include <carlos/fs.h>     // <-- use FS only
#include <carlos/str.h>
#include <carlos/path.h>

#define KAPI_FS_DEBUG 1
#if KAPI_FS_DEBUG
  #define KAPI_DBG(...) kprintf(__VA_ARGS__)
#else
  #define KAPI_DBG(...) do{}while(0)
#endif

static Fs *g_kapi_fs = 0;
static char g_kapi_cwd[128] = "/";

extern void carlos_kexit(int code);

void kapi_bind_fs(Fs *fs){ g_kapi_fs = fs; }

static s32 api_write(const char *buf, size_t len){
  for (size_t i = 0; i < len; i++) kputc(buf[i]);
  return (s32)len;
}

void kapi_set_cwd(const char *cwd){
  if (!cwd || cwd[0] == 0) { g_kapi_cwd[0] = '/'; g_kapi_cwd[1] = 0; return; }

  size_t i = 0;
  for (; i + 1 < sizeof(g_kapi_cwd) && cwd[i]; i++) g_kapi_cwd[i] = cwd[i];
  g_kapi_cwd[i] = 0;

  if (g_kapi_cwd[0] != '/') {
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

// (keep your normalize_abs_path() as-is)

typedef struct {
  CarlosDirEnt *ents;
  u32 max;
  u32 n;
} ListCtx;

static int kapi_list_cb(void *ud, const char name83[13], uint8_t attr, uint32_t size)
{
  ListCtx *c = (ListCtx*)ud;
  if (c->n >= c->max) return 1; // stop

  // copy name
  for (int i = 0; i < CARLOS_NAME_MAX; i++) c->ents[c->n].name[i] = 0;
  for (int i = 0; i < (CARLOS_NAME_MAX - 1) && name83[i]; i++) c->ents[c->n].name[i] = name83[i];

  c->ents[c->n].size = size;
  c->ents[c->n].type = (attr & 0x10) ? 2 : 1; // 2=dir, 1=file
  c->n++;
  return 0;
}

static s32 api_fs_listdir(const char *path, CarlosDirEnt *ents, u32 max_ents)
{
  if (!g_kapi_fs || !ents || max_ents == 0) return -1;

  const char *p = path;
  if (!p || p[0] == 0 || (p[0] == '.' && p[1] == 0)) p = g_kapi_cwd;

  // Build abs path
  char abs[512];
  const int is_abs = (p[0] == '/' || p[0] == '\\');

  if (is_abs) {
    kstrncpy(abs, p, sizeof(abs)-1);
    abs[sizeof(abs)-1] = 0;
  } else {
    const char *cwd = (g_kapi_cwd[0] ? g_kapi_cwd : "/");
    size_t a = kstrlen(cwd);
    size_t b = kstrlen(p);
    if (a == 1 && cwd[0] == '/') a = 0;

    if (1 + a + (a ? 1 : 0) + b + 1 > sizeof(abs)) return -2;

    size_t j = 0;
    abs[j++] = '/';
    for (size_t i = 0; i < a; i++){ char c = cwd[i]; abs[j++] = (c=='\\')?'/':c; }
    if (a) abs[j++] = '/';
    for (size_t i = 0; i < b; i++){ char c = p[i];   abs[j++] = (c=='\\')?'/':c; }
    abs[j] = 0;
  }

  path_normalize_abs(abs, sizeof(abs));

  ListCtx ctx = { .ents = ents, .max = max_ents, .n = 0 };
  int rc = fs_listdir(g_kapi_fs, abs, kapi_list_cb, &ctx);
  if (rc < 0) return (s32)rc;

  return (s32)ctx.n;
}

const CarlosApi g_api = {
  .abi_version = CARLOS_ABI_VERSION,
  .write       = api_write,
  .exit        = carlos_kexit,
  .alloc_pages = api_alloc_pages,
  .free_pages  = api_free_pages,
  .fs_listdir  = api_fs_listdir,
};