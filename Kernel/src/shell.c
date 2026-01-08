#include <stdint.h>
#include <carlos/shell.h>
#include <carlos/uart.h>
#include <carlos/str.h>
#include <carlos/pmm.h>

static void prompt(void){
  uart_puts("carlos> ");
}

static void cmd_help(void){
  uart_puts("Commands:\n");
  uart_puts("  help   - this help\n");
  uart_puts("  mem    - show free pages\n");
  uart_puts("  alloc  - allocate one page\n");
  uart_puts("  halt   - stop CPU\n");
}

static void cmd_mem(void){
  uart_puts("free pages = ");
  // minimal decimal print (local)
  uint64_t v = pmm_free_count();
  char buf[21]; int i=0;
  if (v==0){ uart_putc('0'); uart_puts("\n"); return; }
  while (v>0 && i<(int)sizeof(buf)){ buf[i++]='0'+(v%10); v/=10; }
  for (int j=i-1;j>=0;j--) uart_putc(buf[j]);
  uart_puts("\n");
}

static void cmd_alloc(void){
  void *p = pmm_alloc_page();
  uart_puts("alloc page = ");
  // hex print
  uint64_t x = (uint64_t)(uintptr_t)p;
  static const char *hex="0123456789ABCDEF";
  uart_puts("0x");
  for (int i=60;i>=0;i-=4) uart_putc(hex[(x>>i)&0xF]);
  uart_puts("\n");
}

static void cmd_halt(void){
  uart_puts("halting.\n");
  for(;;) __asm__ volatile ("hlt");
}

static void run_cmd(const char *line){
  if (kstreq(line, "help")) { cmd_help(); return; }
  if (kstreq(line, "mem"))  { cmd_mem();  return; }
  if (kstreq(line, "alloc")){ cmd_alloc();return; }
  if (kstreq(line, "halt")) { cmd_halt(); return; }
  if (kstrlen(line) == 0) return;

  uart_puts("unknown: ");
  uart_puts(line);
  uart_puts("\n");
}

void shell_run(const BootInfo *bi){
  (void)bi;
  uart_puts("Carlos shell ready. Type 'help'.\n");
  prompt();

  char line[128];
  int len = 0;

  while (1){
    char c;
    if (!uart_try_getc(&c)) continue;

    // Enter
    if (c == '\r' || c == '\n'){
      uart_puts("\n");          // <-- ensures a clean new line after Enter
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
        uart_putc('\b'); uart_putc(' '); uart_putc('\b');
      }
      continue;
    }

    // Printable ASCII
    if (c >= 0x20 && c <= 0x7E){
      if (len < (int)sizeof(line)-1){
        line[len++] = c;
        uart_putc(c); // echo
      }
      continue;
    }
  }
}