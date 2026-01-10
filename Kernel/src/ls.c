#include <stddef.h>
#include <carlos/ls.h>
#include <carlos/klog.h>   // kprintf
#include <carlos/str.h>    // if you have kstreq / etc (optional)
#include <carlos/path.h>   


static void cpy0(char *dst, const char *src, uint32_t cap){
  if (!dst || cap == 0) return;
  uint32_t i = 0;
  if (src) for (; i + 1 < cap && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
}

static void join_path(char *out, uint32_t cap, const char *cwd, const char *arg){
  // default cwd
  if (!cwd || !cwd[0]) cwd = "/";

  // no arg => list cwd
  if (!arg || !arg[0]) { kstrncpy(out, cwd, cap); return; }

  // absolute?
  if (path_is_sep(arg[0])) { kstrncpy(out, arg, cap); return; }

  // cwd == "/" ?
  if (cwd[0] == '/' && cwd[1] == 0) {
    // "/" + arg
    if (cap < 2) { if (cap) out[0]=0; return; }
    out[0] = '/';
    out[1] = 0;

    // append arg
    uint32_t j = 1;
    for (uint32_t i=0; arg[i] && j + 1 < cap; i++) out[j++] = arg[i];
    out[j] = 0;
    return;
  }

  // cwd + "/" + arg
  cpy0(out, cwd, cap);
  // append '/'
  uint32_t j = 0;
  while (out[j]) j++;
  if (j + 1 < cap && out[j-1] != '/') out[j++] = '/';
  // append arg
  for (uint32_t i=0; arg[i] && j + 1 < cap; i++) out[j++] = arg[i];
  out[j] = 0;
}

int ls_cmd(Fs *fs, const char *cwd, const char *arg){
  if (!fs) { kprintf("ls: no fs\n"); return -1; }

  char path[256];
  join_path(path, sizeof(path), cwd, arg);

  // For now: just list directory. (If it's a file, fs_list_dir will error.)
  return fs_list_dir(fs, path);
}

int ls_main(Fs *fs, const char *cwd, int argc, char **argv){
  // minimal: `ls` or `ls PATH`
  const char *arg = NULL;
  if (argc >= 2) arg = argv[1];
  return ls_cmd(fs, cwd, arg);
}