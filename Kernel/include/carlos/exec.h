#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct Fs Fs;

typedef struct ExecImage {
  void    *raw;    // kmalloc() return (might differ from base due to align-up)
  void    *base;   // aligned base where image lives
  uint64_t size;
  void    *entry;  // _start
} ExecImage;

int exec_elf_load_pie(const void *file, size_t file_sz, ExecImage *out);
int exec_run_path(Fs *fs, const char *path, int argc, char **argv, const char *cwd);