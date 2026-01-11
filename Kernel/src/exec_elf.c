// Kernel/src/exec_elf.c
#include <stdint.h>
#include <stddef.h>

#include <carlos/exec.h>
#include <carlos/kmem.h>

static inline void memclr(void *p, size_t n){ __builtin_memset(p, 0, n); }
static inline void memcp(void *d, const void *s, size_t n){ __builtin_memcpy(d, s, n); }

#define EI_NIDENT 16
typedef struct __attribute__((packed)) {
  unsigned char e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct __attribute__((packed)) {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
} Elf64_Phdr;

typedef struct __attribute__((packed)) {
  int64_t  d_tag;
  union { uint64_t d_val; uint64_t d_ptr; } d_un;
} Elf64_Dyn;

typedef struct __attribute__((packed)) {
  uint64_t r_offset;
  uint64_t r_info;
  int64_t  r_addend;
} Elf64_Rela;

#define PT_LOAD     1
#define PT_DYNAMIC  2

#define ET_DYN      3
#define EM_X86_64   62

#define DT_NULL     0
#define DT_RELA     7
#define DT_RELASZ   8
#define DT_RELAENT  9

#define ELF64_R_TYPE(i) ((uint32_t)((i) & 0xFFFFFFFFu))
#define R_X86_64_RELATIVE 8

static void *kmem_align_up(void *p, uint64_t align){
  uint64_t x = (uint64_t)(uintptr_t)p;
  uint64_t y = (x + (align - 1)) & ~(align - 1);
  return (void*)(uintptr_t)y;
}

int exec_elf_load_pie(const void *file, size_t file_sz, ExecImage *out)
{
  if (!file || file_sz < sizeof(Elf64_Ehdr) || !out) return -1;
  *out = (ExecImage){0};

  const Elf64_Ehdr *eh = (const Elf64_Ehdr*)file;

  if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') return -11;
  if (eh->e_ident[4] != 2) return -12; // 64-bit
  if (eh->e_ident[5] != 1) return -13; // little
  if (eh->e_machine != EM_X86_64) return -14;

  if (eh->e_type != ET_DYN) return -15; // PIE only

  if (eh->e_phoff == 0 || eh->e_phnum == 0) return -16;
  if (eh->e_phentsize != sizeof(Elf64_Phdr)) return -17;
  if (eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(Elf64_Phdr) > file_sz) return -18;

  const Elf64_Phdr *ph = (const Elf64_Phdr*)((const uint8_t*)file + eh->e_phoff);

  uint64_t minv = ~0ull, maxv = 0;
  const Elf64_Phdr *dyn_ph = 0;

  for (uint16_t i = 0; i < eh->e_phnum; i++){
    if (ph[i].p_type == PT_LOAD) {
      if (ph[i].p_memsz == 0) continue;
      if (ph[i].p_offset + ph[i].p_filesz > file_sz) return -19;
      if (ph[i].p_vaddr < minv) minv = ph[i].p_vaddr;
      uint64_t end = ph[i].p_vaddr + ph[i].p_memsz;
      if (end > maxv) maxv = end;
    } else if (ph[i].p_type == PT_DYNAMIC) {
      dyn_ph = &ph[i];
    }
  }
  if (minv == ~0ull) return -20;

  uint64_t img_sz = maxv - minv;

  void *raw = kmalloc((size_t)img_sz + 0x1000);
  if (!raw) return -21;

  uint8_t *base = (uint8_t*)kmem_align_up(raw, 0x1000);
  memclr(base, (size_t)img_sz);

  // load segments
  for (uint16_t i = 0; i < eh->e_phnum; i++){
    if (ph[i].p_type != PT_LOAD) continue;
    if (ph[i].p_filesz == 0) continue;

    uint64_t dst_off = ph[i].p_vaddr - minv;
    if (dst_off + ph[i].p_filesz > img_sz) return -22;

    memcp(base + dst_off, (const uint8_t*)file + ph[i].p_offset, (size_t)ph[i].p_filesz);
  }

  // relocations: RELA + R_X86_64_RELATIVE only
  if (dyn_ph) {
    uint64_t dyn_off = dyn_ph->p_vaddr - minv;
    if (dyn_off + dyn_ph->p_memsz > img_sz) return -23;

    Elf64_Dyn *dyn = (Elf64_Dyn*)(base + dyn_off);

    uint64_t rela_v = 0, rela_sz = 0, rela_ent = sizeof(Elf64_Rela);
    for (; dyn->d_tag != DT_NULL; dyn++){
      if (dyn->d_tag == DT_RELA)    rela_v  = dyn->d_un.d_ptr;
      if (dyn->d_tag == DT_RELASZ)  rela_sz = dyn->d_un.d_val;
      if (dyn->d_tag == DT_RELAENT) rela_ent = dyn->d_un.d_val;
    }

    if (rela_v && rela_sz) {
      if (rela_ent != sizeof(Elf64_Rela)) return -24;

      uint64_t rela_off = rela_v - minv;
      if (rela_off + rela_sz > img_sz) return -25;

      Elf64_Rela *r = (Elf64_Rela*)(base + rela_off);
      uint64_t n = rela_sz / sizeof(Elf64_Rela);

      for (uint64_t i = 0; i < n; i++){
        if (ELF64_R_TYPE(r[i].r_info) != R_X86_64_RELATIVE) continue;

        uint64_t off = r[i].r_offset - minv;
        if (off + 8 > img_sz) return -26;

        *(uint64_t*)(base + off) =
          (uint64_t)(uintptr_t)(base + (uint64_t)r[i].r_addend);
      }
    }
  }

  uint64_t entry_off = eh->e_entry - minv;
  if (entry_off >= img_sz) return -27;

  out->raw   = raw;                 // for future freeing if you implement kfree
  out->base  = base;
  out->size  = img_sz;
  out->entry = (void*)(base + entry_off);
  return 0;
}