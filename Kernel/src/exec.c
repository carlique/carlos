// Kernel/src/exec.c
#include <stdint.h>
#include <stddef.h>

#include <carlos/exec.h>
#include <carlos/fs.h>
#include <carlos/klog.h>
#include <carlos/kmem.h>
#include <carlos/kapi.h>   // g_api
#include <carlos/pmm.h>    // optional: pmm_free_count() for debug prints

// exec.c logging (runtime controlled by g_klog_level + g_klog_mask)
#define EXEC_TRACE(...) KLOG(KLOG_MOD_EXEC, KLOG_TRACE, __VA_ARGS__)
#define EXEC_DBG(...)   KLOG(KLOG_MOD_EXEC, KLOG_DBG,   __VA_ARGS__)
#define EXEC_INFO(...)  KLOG(KLOG_MOD_EXEC, KLOG_INFO,  __VA_ARGS__)
#define EXEC_WARN(...)  KLOG(KLOG_MOD_EXEC, KLOG_WARN,  __VA_ARGS__)
#define EXEC_ERR(...)   KLOG(KLOG_MOD_EXEC, KLOG_ERR,   __VA_ARGS__)

uint64_t g_exec_saved_rsp = 0;
int      g_exec_exit_code = 0;

extern void kapi_set_cwd(const char *cwd);

// exec_s.S
int  exec_enter(void *entry, void *stack_top, void *api, int argc, char **argv);
void carlos_kexit(int code);

int exec_run_path(Fs *fs, const char *path, int argc, char **argv, const char *cwd)
{
  int rc = 0;
  int code = 0;

  void *file = 0;
  uint32_t file_sz = 0;
  ExecImage img = (ExecImage){0};
  uint8_t *stk = 0;

  if (!fs || !path) return -1;

  // One-call load (alloc + read) owned by FS layer
  rc = fs_read_file(fs, path, &file, &file_sz);
  if (rc != 0) {
    EXEC_ERR("exec: fs_read_file rc=%d path=%s\n", rc, path);
    return rc;
  }

  EXEC_DBG("exec: loaded %s size=%u (pmm_free=%llu) buf=%p\n",
           path, (unsigned)file_sz,
           (unsigned long long)pmm_free_count(),
           file);

  if (file_sz == 0 || !file) {
    // treat empty as error for exec
    if (file) kfree(file);
    return -2;
  }

  rc = exec_elf_load_pie(file, (size_t)file_sz, &img);

  // File buffer is no longer needed after load attempt
  kfree(file);
  file = 0;
  file_sz = 0;

  if (rc != 0) {
    EXEC_ERR("exec: elf_load rc=%d path=%s\n", rc, path);
    // img is guaranteed empty on failure with current exec_elf_load_pie()
    return rc;
  }

  stk = (uint8_t*)kmalloc(64 * 1024);
  if (!stk) { rc = -4; goto cleanup; }

  EXEC_INFO("exec: %s base=%p entry=%p\n", path, img.base, img.entry);

  kapi_set_cwd(cwd);
  code = exec_enter(img.entry, stk + 64 * 1024, (void*)&g_api, argc, argv);

  EXEC_INFO("\n[app exit %d]\n", code);

cleanup:
  if (stk) kfree(stk);
  if (img.raw) kfree(img.raw);

  if (rc != 0) return rc;
  return code;
}