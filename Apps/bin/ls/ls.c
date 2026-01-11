// Apps/bin/ls/ls.c
#include <carlos/api.h>
#include <carlos/uapi/fs.h>
#include <carlos/uapi/types.h>

extern const CarlosApi *carlos_api;

static void write_str(const char *s){
  size_t n = 0;
  while (s[n]) n++;
  (void)carlos_api->write(s, n);
}

static void write_ch(char c){
  (void)carlos_api->write(&c, 1);
}

static void write_u32(u32 v){
  char buf[16];
  int i = 0;
  if (v == 0) { write_ch('0'); return; }
  while (v && i < (int)sizeof(buf)) {
    buf[i++] = (char)('0' + (v % 10));
    v /= 10;
  }
  while (i--) write_ch(buf[i]);
}

static void write_s32(s32 v){
  if (v < 0) { write_ch('-'); write_u32((u32)(-v)); }
  else       { write_u32((u32)v); }
}

int main(int argc, char **argv){
  // Default: list CWD (kernel-side semantics for NULL/""/"." should handle it)
  const char *path = ".";

  if (argc >= 2 && argv[1] && argv[1][0]) path = argv[1];

  CarlosDirEnt ents[128];
  s32 n = carlos_api->fs_listdir(path, ents, 128);
  if (n < 0) {
    write_str("ls: fs_listdir failed rc=");
    write_s32(n);
    write_ch('\n');
    return 1;
  }

  for (s32 i = 0; i < n; i++){
    if (ents[i].type == 2) write_str("D ");
    else                  write_str("F ");

    write_str(ents[i].name);

    if (ents[i].type != 2) {
      write_str("  ");
      write_u32(ents[i].size);
    }
    write_ch('\n');
  }

  return 0;
}