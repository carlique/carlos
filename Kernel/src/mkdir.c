#include <carlos/mkdir.h>
#include <carlos/klog.h>

int mkdir_cmd(Fs *fs, const char *arg){
  if (!fs) return -1;
  if (!arg || !arg[0]) { kprintf("mkdir: missing operand\n"); return -2; }

  int rc = fs_mkdir(fs, arg);   // <-- raw user path
  if (rc != 0){
    kprintf("mkdir: failed rc=%d (%s)\n", rc, arg);
    return rc;
  }

  return 0;
}

int mkdir_main(Fs *fs, int argc, char **argv){
  if (argc < 2) { kprintf("mkdir: missing operand\n"); return 2; }
  return mkdir_cmd(fs, argv[1]);
}