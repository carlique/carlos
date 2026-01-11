#include <stdint.h>
#include <carlos/shell.h>
#include <carlos/str.h>
#include <carlos/pmm.h>
#include <carlos/kbd.h>
#include <carlos/klog.h>
#include <carlos/uart.h>
#include <carlos/fbcon.h>
#include <carlos/time.h>
#include <carlos/pci.h>
#include <carlos/ahci.h>
#include <carlos/fs.h>
#include <carlos/path.h>
#include <carlos/exec.h>

#include <carlos/ls.h>
#include <carlos/mkdir.h>

#define SHELL_MAX_ARGS 8

static Fs *g_fs = NULL; 

static char g_cwd[CARLOS_PATH_MAX] = "/";   // start at root

static inline void outb(uint16_t port, uint8_t val){
  __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
  uint8_t r;
  __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

static const char* skip_ws(const char *s){
  while (s && (*s == ' ' || *s == '\t')) s++;
  return s;
}

static int split_args(char *s, char **argv, int max_args){
  int argc = 0;
  if (!s || !argv || max_args <= 0) return 0;

  s = (char*)skip_ws(s);
  while (*s && argc < max_args) {
    argv[argc++] = s;

    // advance to end of token
    while (*s && *s != ' ' && *s != '\t') s++;

    // terminate token
    if (*s == 0) break;
    *s++ = 0;

    // skip whitespace to next token
    s = (char*)skip_ws(s);
  }

  return argc;
}

static uint64_t parse_u64(const char *s){
  s = skip_ws(s);
  uint64_t v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (uint64_t)(*s - '0');
    s++;
  }
  return v;
}

static int hexval(char c){
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

static int parse_hex_u8_1or2(const char *s, uint8_t *out, const char **end){
  int a = hexval(*s);
  if (a < 0) return -1;
  int b = hexval(*(s+1));
  if (b >= 0) { *out = (uint8_t)((a<<4) | b); if (end) *end = s+2; }
  else        { *out = (uint8_t)a;            if (end) *end = s+1; }
  return 0;
}

// Accept "00:1f.2" or "0:1f.2"
static int parse_bdf(const char *arg, uint8_t *bus, uint8_t *dev, uint8_t *fun){
  arg = skip_ws(arg);
  if (!arg || !*arg) return -1;

  const char *p = arg;
  if (parse_hex_u8_1or2(p, bus, &p) != 0) return -1;
  if (*p != ':') return -1;
  p++;

  if (parse_hex_u8_1or2(p, dev, &p) != 0) return -1;
  if (*p != '.') return -1;
  p++;

  int fv = hexval(*p);
  if (fv < 0) return -1;
  *fun = (uint8_t)fv;
  return 0;
}

static int parse_dec_u8(const char *s, uint8_t *out, const char **end){
  s = skip_ws(s);
  if (!s || *s < '0' || *s > '9') return -1;
  uint32_t v = 0;
  while (*s >= '0' && *s <= '9') { v = v*10 + (uint32_t)(*s - '0'); s++; }
  if (v > 255) return -1;
  *out = (uint8_t)v;
  if (end) *end = s;
  return 0;
}

static int parse_bdf_dec3(const char *arg, uint8_t *bus, uint8_t *dev, uint8_t *fun){
  const char *p = skip_ws(arg);
  if (!p || !*p) return -1;

  if (parse_dec_u8(p, bus, &p) != 0) return -1;
  p = skip_ws(p);
  if (!*p) return -1;

  if (parse_dec_u8(p, dev, &p) != 0) return -1;
  p = skip_ws(p);
  if (!*p) return -1;

  if (parse_dec_u8(p, fun, &p) != 0) return -1;

  // optional trailing whitespace only
  p = skip_ws(p);
  if (*p != 0) return -1;

  return 0;
}

static void hexdump_512(const void *p){
  const uint8_t *b = (const uint8_t*)p;

  for (uint32_t i = 0; i < 512; i += 16){
    kprintf("0x%08x: ", i);

    for (int j = 0; j < 16; j++){
      kprintf("%02x ", (unsigned)b[i + j]);
    }

    kputs(" |");
    for (int j = 0; j < 16; j++){
      char c = (char)b[i + j];
      if (c < 32 || c > 126) c = '.';
      kputc(c);
    }
    kputs("|\n");
  }
}

static void cpu_halt_forever(void){
  for(;;) __asm__ volatile ("hlt");
}

static void path_join(char *out, size_t cap, const char *cwd, const char *arg){
  // if arg is absolute -> use it, else cwd + "/" + arg
  size_t j = 0;
  if (!out || cap == 0) return;
  out[0] = 0;

  const char *p = arg;
  while (p && *p && path_is_sep(*p)) { out[j++] = '/'; break; }

  if (!(arg && (arg[0]=='/' || arg[0]=='\\'))) {
    // copy cwd
    for (size_t i=0; cwd && cwd[i] && j+1<cap; i++){
      char c = cwd[i];
      out[j++] = (c=='\\') ? '/' : c;
    }
    if (j==0) out[j++] = '/';
    if (j>1 && out[j-1] != '/' && j+1<cap) out[j++] = '/';
    p = arg;
  }

  // copy arg
  for (size_t i=0; p && p[i] && j+1<cap; i++){
    char c = p[i];
    out[j++] = (c=='\\') ? '/' : c;
  }
  out[j] = 0;
}

static void prompt(void){
  kprintf("carlos:%s> ", g_cwd);
}

static void cmd_help(void){
  kputs("Commands:\n");
  kputs("  help   - this help\n");
  kputs("  mem    - show free pages\n");
  kputs("  alloc  - allocate one page\n");
  kputs("  clear  - clear screen\n");
  kputs("  halt   - stop CPU\n");
  kputs("  reboot - reboot machine\n");
  kputs("  pf      - trigger a page fault (test IDT)\n");
  kputs("  time    - show current time (ns)\n");
  kputs("  sleep N - sleep N milliseconds\n");
  kputs("  lspci   - list PCI devices\n");
  kputs("  pcidump BB:DD.F - dump PCI config of device\n");
  kputs("  ahci    - probe for AHCI controller\n");
  kputs("  ahci_read <port> <lba> [count] - read sectors via AHCI and hexdump\n");
  kputs("  log [lvl] [mask] - set logger (lvl: err|warn|info|dbg|trace)\n");
}

static void cmd_cwd(void){
  kprintf("%s\n", g_cwd);
}

static void cmd_cd(const char *arg){
  if (!g_fs) { kprintf("cd: fs not mounted\n"); return; }

  if (!arg || arg[0] == 0) {
    g_cwd[0] = '/'; g_cwd[1] = 0;
    return;
  }

  char cand[256];
  path_join(cand, sizeof(cand), g_cwd, arg);
  path_normalize_abs(cand, sizeof(cand));

  FsStat st;
  int rc = fs_stat(g_fs, cand, &st);
  if (rc != 0) { kprintf("cd: not found: %s\n", cand); return; }
  if (st.type != FS_TYPE_DIR) { kprintf("cd: not a directory: %s\n", cand); return; }

  kstrncpy(g_cwd, cand, sizeof(g_cwd)-1);
  g_cwd[sizeof(g_cwd)-1] = 0;
}

static void cmd_log(int argc, char **argv){
  if (argc == 1) { klog_print_state(); return; }
  if (argc == 2) {
    if (klog_set_level_str(argv[1]) != 0) { klog_print_help(); return; }
    klog_print_state();
    return;
  }
  if (argc >= 3) {
    if (klog_set_level_str(argv[1]) != 0) { klog_print_help(); return; }
    if (klog_set_mask_str(argv[2])  != 0) { klog_print_help(); return; }
    klog_print_state();
    return;
  }
}

static void cmd_mem(void){
  kprintf("free pages = %llu\n", pmm_free_count());
}

static void cmd_alloc(void){
  void *p = pmm_alloc_page();
  kprintf("alloc page = %p\n", p);
}

static void cmd_clear(void){
  // Clear framebuffer
  fbcon_clear();

  // Also clear the serial terminal (ANSI escape)
  uart_puts("\x1b[2J\x1b[H");
}

static void cmd_halt(void){
  kputs("halting.\n");
  for(;;) __asm__ volatile ("hlt");
}

static void cmd_reboot(void){
  kputs("rebooting...\n");

  __asm__ volatile ("cli");

  // Wait until input buffer is clear (bit 1 == 0)
  for (int i = 0; i < 100000; i++){
    if ((inb(0x64) & 0x02) == 0) break;
  }

  // Pulse CPU reset line via 8042 controller
  outb(0x64, 0xFE);

  // If it didn't reboot, just halt.
  cpu_halt_forever();
}

static void cmd_pf(void){
  kprintf("Triggering page fault...\n");
  volatile uint64_t *p = (volatile uint64_t*)0xDEADBEEF000ULL;
  *p = 1; // should fault -> #PF vec=14, CR2 = that address
}

static void cmd_time(void){
  kprintf("now_ns=%llu\n", time_now_ns());
}

static void cmd_sleep(const char *arg){
  uint64_t ms = parse_u64(arg);
  kprintf("sleep %llu ms...\n", ms);
  time_sleep_ms(ms);
  kprintf("done\n");
}

static void cmd_lspci(void){
  pci_list();
}

static void cmd_pcidump(const char *arg){
  uint8_t b=0,d=0,f=0;

  if (parse_bdf(arg, &b, &d, &f) != 0) {
    if (parse_bdf_dec3(arg, &b, &d, &f) != 0) {
      kprintf("usage: pcidump BB:DD.F  |  pcidump <bus> <dev> <fun>\n");
      return;
    }
  }

  pci_dump_bdf(b, d, f);
}

static void cmd_ahci(const char *arg){
  // v1: ignore arg and just auto-probe
  (void)arg;
  ahci_probe();
}

static void cmd_ahci_read(const char *arg){
  // usage: ahci_read <port> <lba> [count]
  uint8_t port = 0;
  uint64_t lba = 0;
  uint64_t cnt = 1;

  const char *p = skip_ws(arg);
  if (!p || !*p) { kputs("usage: ahci_read <port> <lba> [count]\n"); return; }

  // port (decimal u8)
  uint8_t pv=0; const char *e=0;
  if (parse_dec_u8(p, &pv, &e) != 0) { kputs("bad port\n"); return; }
  port = pv; p = skip_ws(e);

  // lba (decimal u64)
  lba = parse_u64(p);
  while (*p && *p != ' ' && *p != '\t') p++;
  p = skip_ws(p);

  // optional count
  if (p && *p) cnt = parse_u64(p);
  if (cnt == 0) cnt = 1;
  if (cnt > 8) cnt = 8; // keep it small for now

  // allocate buffer (one page is enough for up to 8 sectors = 4096 bytes)
  void *buf = pmm_alloc_page();
  if (!buf) { kputs("no mem\n"); return; }

  int rc = ahci_read(port, lba, (uint32_t)cnt, buf);
  kprintf("ahci_read rc=%d\n", (int64_t)rc);

  if (rc == 0){
    hexdump_512(buf);
  }

  pmm_free_page(buf);
}

static void run_cmd(char *line){
  if (!line) return;

  // Build argc/argv from the line (in-place)
  char *argv[SHELL_MAX_ARGS];
  int argc = split_args(line, argv, SHELL_MAX_ARGS);

  if (argc == 0) return;

  char *cmd = argv[0];
  char *arg = (argc >= 2) ? argv[1] : 0;

  // Dispatch
  if (kstreq(cmd, "help"))   { cmd_help();   return; }
  if (kstreq(cmd, "cwd")) { cmd_cwd(); return; }
  if (kstreq(cmd, "pwd")) { cmd_cwd(); return; }
  if (kstreq(cmd, "cd")) { cmd_cd(arg); return; }

  if (kstreq(cmd, "log")) { cmd_log(argc, argv); return; }

  if (kstreq(cmd, "mem"))    { cmd_mem();    return; }
  if (kstreq(cmd, "alloc"))  { cmd_alloc();  return; }
  if (kstreq(cmd, "clear"))  { cmd_clear();  return; }
  if (kstreq(cmd, "halt"))   { cmd_halt();   return; }
  if (kstreq(cmd, "reboot")) { cmd_reboot(); return; }

  if (kstreq(cmd, "pf"))     { cmd_pf();     return; }
  if (kstreq(cmd, "time"))   { cmd_time();   return; }
  if (kstreq(cmd, "sleep"))  { cmd_sleep(arg); return; }

  if (kstreq(cmd, "lspci"))   { cmd_lspci(); return; }
  if (kstreq(cmd, "pcidump")) { cmd_pcidump(arg); return; }
  if (kstreq(cmd, "ahci"))    { cmd_ahci(arg); return; }
  if (kstreq(cmd, "ahci_read")) { cmd_ahci_read(arg); return; }

  if (kstreq(cmd, "ls")) {
    if (!g_fs) { kprintf("ls: fs not mounted\n"); return; }

    // forward shell args to userland
    char *uargv[SHELL_MAX_ARGS + 1];
    int uargc = 0;

    static char arg0[] = "ls";
    uargv[uargc++] = arg0;

    // copy everything after "ls" (so "ls bin" becomes argv[1]="bin")
    for (int i = 1; i < argc && uargc < SHELL_MAX_ARGS; i++){
      uargv[uargc++] = argv[i];
    }
    uargv[uargc] = 0;

    int code = exec_run_path(g_fs, "BIN/LS.ELF", uargc, uargv, g_cwd);
    kprintf("runls: exit=%d\n", code);
    return;
  }

  if (kstreq(cmd, "mkdir")) {
    if (!g_fs) { kprintf("mkdir: fs not mounted\n"); return; }
    (void)mkdir_cmd(g_fs, arg);
    return;
  }

  kputs("unknown: ");
  kputs(cmd);
  kputs("\n");
}

void shell_run(const BootInfo *bi, Fs *fs) {
  (void)bi;

  g_fs = fs;

  kputs("Carlos shell ready. Type 'help'.\n");
  prompt();

  char line[128];
  int len = 0;

  kbd_init();

  while (1){
    char c;
    if (!kbd_try_getc(&c) && !uart_try_getc(&c)) continue;

    // Enter
    if (c == '\r' || c == '\n'){
      kputs("\n");          // <-- ensures a clean new line after Enter
      line[len] = 0;
      run_cmd(line);
      len = 0;
      prompt();
      continue;
    }

    // Backspace (0x08) or DEL (0x7F)
    if (c == 0x08 || c == 0x7F){
      if (len > 0){
        len--;
        // erase on terminal: back, space, back
        kputc('\b'); kputc(' '); kputc('\b');
      }
      continue;
    }

    // Printable ASCII
    if (c >= 0x20 && c <= 0x7E){
      if (len < (int)sizeof(line)-1){
        line[len++] = c;
        kputc(c); // echo
      }
      continue;
    }
  }
}