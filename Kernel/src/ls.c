#include <carlos/ls.h>
#include <carlos/klog.h>

static void usage(void){
  kprintf("usage: ls [PATH]\n");
}

int ls_cmd(Fs *fs, const char *arg)
{
  if (!fs) return -1;

  if (arg && arg[0] == '-' && arg[1] == 'h' && arg[2] == 0) {
    usage();
    return 0;
  }

  const char *path = (arg && arg[0]) ? arg : "/";
  int rc = fs_list_dir(fs, path);
  if (rc != 0) kprintf("ls: failed rc=%d\n", rc);
  return rc;
}

int ls_main(Fs *fs, int argc, char **argv)
{
  const char *arg = (argc >= 2) ? argv[1] : 0;
  return ls_cmd(fs, arg);
}