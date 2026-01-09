#include <carlos/kapi.h>
#include <carlos/klog.h>
#include <carlos/pmm.h>

// Minimal write: route to kputc so it hits UART + framebuffer
static s32 api_write(const char *buf, size_t len){
  for (size_t i = 0; i < len; i++) kputc(buf[i]);
  return (s32)len;
}

// Minimal exit: for now just print (later: terminate task)
static void api_exit(s32 code){
  kprintf("\n[app exit %d]\n", (int)code);
}

// Minimal pages: for now just return 1 page when pages==1
// You can expand to multiple pages later.
static void* api_alloc_pages(size_t pages){
  (void)pages;
  return pmm_alloc_page();
}

static void api_free_pages(void *p, size_t pages){
  (void)pages;
  pmm_free_page(p);
}

const CarlosApi g_api = {
  .abi_version = CARLOS_ABI_VERSION,
  .write       = api_write,
  .exit        = api_exit,
  .alloc_pages = api_alloc_pages,
  .free_pages  = api_free_pages,
};