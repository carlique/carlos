#include <carlos/mkdir.h>
#include <carlos/klog.h>

static char upc(char c){ return (c>='a' && c<='z') ? (char)(c - 'a' + 'A') : c; }

// normalize similar to your fs.c: collapse //, \ -> /, uppercase
static void norm_path83(const char *in, char *out, unsigned cap){
  unsigned j = 0;
  if (!cap) return;
  out[0] = 0;

  while (in && (*in=='/' || *in=='\\')) in++;

  for (unsigned i=0; in && in[i] && j + 1 < cap; i++){
    char c = in[i];
    if (c == '\\') c = '/';

    if (c == '/'){
      if (j == 0 || out[j-1] == '/') continue;
      out[j++] = '/';
      continue;
    }

    out[j++] = upc(c);
  }

  if (j > 0 && out[j-1] == '/') j--;
  out[j] = 0;
}

int mkdir_cmd(Fs *fs, const char *arg){
  if (!fs) return -1;
  if (!arg || !arg[0]) { kprintf("mkdir: missing operand\n"); return -2; }

  char p83[256];
  norm_path83(arg, p83, sizeof(p83));
  if (!p83[0]) { kprintf("mkdir: bad path\n"); return -3; }

  int rc = fat16_mkdir_path83(&fs->fat, p83);
  if (rc != 0){
    kprintf("mkdir: failed rc=%d (%s)\n", rc, p83);
    return rc;
  }

  return 0;
}

int mkdir_main(Fs *fs, int argc, char **argv){
  if (argc < 2) { kprintf("mkdir: missing operand\n"); return 2; }
  return mkdir_cmd(fs, argv[1]);
}