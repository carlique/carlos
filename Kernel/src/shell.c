#include <stdint.h>
#include <carlos/shell.h>
#include <carlos/uart.h>
#include <carlos/str.h>
#include <carlos/pmm.h>
#include <carlos/kbd.h>
#include <carlos/klog.h>
#include <carlos/fbcon.h>

static inline void outb(uint16_t port, uint8_t val){
  __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port){
  uint8_t r;
  __asm__ volatile ("inb %1, %0" : "=a"(r) : "Nd"(port));
  return r;
}

static void cpu_halt_forever(void){
  for(;;) __asm__ volatile ("hlt");
}

static void prompt(void){
  kputs("carlos> ");
}

static void cmd_help(void){
  kputs("Commands:\n");
  kputs("  help   - this help\n");
  kputs("  mem    - show free pages\n");
  kputs("  alloc  - allocate one page\n");
  kputs("  clear  - clear screen\n");
  kputs("  halt   - stop CPU\n");
  kputs("  reboot - reboot machine\n");
}

static void cmd_mem(void){
  kputs("free pages = ");
  // minimal decimal print (local)
  uint64_t v = pmm_free_count();
  char buf[21]; int i=0;
  if (v==0){ kputc('0'); kputs("\n"); return; }
  while (v>0 && i<(int)sizeof(buf)){ buf[i++]='0'+(v%10); v/=10; }
  for (int j=i-1;j>=0;j--) kputc(buf[j]);
  kputs("\n");
}

static void cmd_alloc(void){
  void *p = pmm_alloc_page();
  kputs("alloc page = ");
  // hex print
  uint64_t x = (uint64_t)(uintptr_t)p;
  static const char *hex="0123456789ABCDEF";
  kputs("0x");
  for (int i=60;i>=0;i-=4) kputc(hex[(x>>i)&0xF]);
  kputs("\n");
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

static void run_cmd(const char *line){
  if (kstreq(line, "help")) { cmd_help(); return; }
  if (kstreq(line, "mem"))  { cmd_mem();  return; }
  if (kstreq(line, "alloc")){ cmd_alloc();return; }
  if (kstreq(line, "clear")) { cmd_clear(); return; }
  if (kstreq(line, "halt")) { cmd_halt(); return; }
  if (kstreq(line, "reboot")) { cmd_reboot(); return; }
  if (kstrlen(line) == 0) return;

  kputs("unknown: ");
  kputs(line);
  kputs("\n");
}

void shell_run(const BootInfo *bi){
  (void)bi;
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