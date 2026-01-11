// Kernel/src/exec.c
#include <stdint.h>
#include <stddef.h>

#include <carlos/exec.h>
#include <carlos/klog.h>
#include <carlos/kmem.h>
#include <carlos/kapi.h>   // g_api
#include <carlos/fat16.h>
#include <carlos/pmm.h>

uint64_t g_exec_saved_rsp = 0;
int      g_exec_exit_code = 0;

extern void kapi_set_cwd(const char *cwd);

// exec_s.S
int  exec_enter(void *entry, void *stack_top, void *api, int argc, char **argv);
void carlos_kexit(int code);

// Reads whole file into kmalloc buffer
static int read_entire(Fs *fs, const char *path, void **out_buf, uint32_t *out_size){
  if (!fs || !path || !out_buf || !out_size) return -1;
  *out_buf = 0; *out_size = 0;

  const char *p = path;
  if (p[0] == '/' || p[0] == '\\') p++;

  uint16_t clus = 0;
  uint8_t  attr = 0;
  uint32_t size = 0;

  int rc = fat16_stat_path83(&fs->fat, p, &clus, &attr, &size);
  if (rc != 0) return rc;
  if (attr & 0x10) return -2;

  kprintf("exec: want file size=%u bytes (pmm_free=%llu)\n",
          (unsigned)size, (unsigned long long)pmm_free_count());

  void *buf = kmalloc(size);
  kprintf("exec: kmalloc(%u) -> %p\n", (unsigned)size, buf);
  if (!buf) return -3;

  rc = fat16_read_file_by_clus(&fs->fat, clus, 0, size, buf);
  if (rc != 0) return rc;

  *out_buf = buf;
  *out_size = size;
  return 0;
}

int exec_run_path(Fs *fs, const char *path, int argc, char **argv, const char *cwd)
{
  if (!fs || !path) return -1;

  void *file = 0;
  uint32_t file_sz = 0;

  int rc = read_entire(fs, path, &file, &file_sz);
  if (rc != 0) {
    kprintf("exec: read failed rc=%d path=%s\n", rc, path);
    return rc;
  }

  ExecImage img;
  rc = exec_elf_load_pie(file, (size_t)file_sz, &img);
  if (rc != 0) {
    kprintf("exec: elf_load rc=%d path=%s\n", rc, path);
    return rc;
  }

  uint8_t *stk = (uint8_t*)kmalloc(64*1024);
  if (!stk) return -4;

  kprintf("exec: %s base=%p entry=%p\n", path, img.base, img.entry);

  kapi_set_cwd(cwd);          // make syscalls relative-aware
  int code = exec_enter(img.entry, stk + 64*1024, (void*)&g_api, argc, argv);

  kprintf("\n[app exit %d]\n", code);
  return code;
}