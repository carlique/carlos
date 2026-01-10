// src/part_gpt.c
#include <carlos/part.h>
#include <carlos/disk.h>
#include <stdint.h>
#include <stddef.h>

static inline uint32_t rd32(const void *p){
  const uint8_t *b = (const uint8_t*)p;
  return (uint32_t)b[0]
       | ((uint32_t)b[1] << 8)
       | ((uint32_t)b[2] << 16)
       | ((uint32_t)b[3] << 24);
}
static inline uint64_t rd64(const void *p){
  const uint8_t *b = (const uint8_t*)p;
  return (uint64_t)rd32(b) | ((uint64_t)rd32(b+4) << 32);
}

typedef struct __attribute__((packed)) {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t  Data4[8];
} Guid;

typedef struct __attribute__((packed)) {
  uint8_t  Sig[8];            // "EFI PART"
  uint32_t Rev;
  uint32_t HeaderSize;
  uint32_t HeaderCrc32;
  uint32_t Reserved;
  uint64_t CurrentLBA;
  uint64_t BackupLBA;
  uint64_t FirstUsableLBA;
  uint64_t LastUsableLBA;
  Guid     DiskGuid;
  uint64_t PartEntryLBA;
  uint32_t NumPartEntries;
  uint32_t SizeOfPartEntry;
  uint32_t PartEntryCrc32;
} GptHdr;

typedef struct __attribute__((packed)) {
  Guid     TypeGuid;
  Guid     PartGuid;          // PARTUUID
  uint64_t FirstLBA;
  uint64_t LastLBA;
  uint64_t Attr;
  uint16_t Name[36];
} GptEntry;

static int is_hex(char c){
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}
static int hex_val(char c){
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static int read_u32_8(const char *p, uint32_t *v){
  uint32_t x = 0;
  for (int i=0;i<8;i++){
    int h = hex_val(p[i]);
    if (h < 0) return -10;
    x = (x<<4) | (uint32_t)h;
  }
  *v = x;
  return 0;
}
static int read_u16_4(const char *p, uint16_t *v){
  uint32_t x = 0;
  for (int i=0;i<4;i++){
    int h = hex_val(p[i]);
    if (h < 0) return -11;
    x = (x<<4) | (uint32_t)h;
  }
  *v = (uint16_t)x;
  return 0;
}
static int read_byte_2(const char *p, uint8_t *v){
  int h0 = hex_val(p[0]); if (h0 < 0) return -12;
  int h1 = hex_val(p[1]); if (h1 < 0) return -13;
  *v = (uint8_t)((h0<<4) | h1);
  return 0;
}

static int guid_parse(const char *s, Guid *out){
  if (!s || !out) return -1;
  if (*s == '{') s++;

  // validate hyphen positions + hex digits
  for (int i = 0; i < 36; i++){
    char c = s[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return -2;
    } else {
      if (!is_hex(c)) return -3;
    }
  }

  uint32_t d1=0; uint16_t d2=0, d3=0;
  int rc;

  rc = read_u32_8(s+0, &d1);  if (rc) return rc;
  rc = read_u16_4(s+9, &d2);  if (rc) return rc;
  rc = read_u16_4(s+14,&d3);  if (rc) return rc;

  out->Data1 = d1;
  out->Data2 = d2;
  out->Data3 = d3;

  rc = read_byte_2(s+19, &out->Data4[0]); if (rc) return rc;
  rc = read_byte_2(s+21, &out->Data4[1]); if (rc) return rc;

  for (int i=0;i<6;i++){
    rc = read_byte_2(s+24+i*2, &out->Data4[2+i]);
    if (rc) return rc;
  }

  return 0;
}

static int guid_eq(const Guid *a, const Guid *b){
  const uint8_t *x = (const uint8_t*)a;
  const uint8_t *y = (const uint8_t*)b;
  for (size_t i=0;i<sizeof(Guid);i++) if (x[i] != y[i]) return 0;
  return 1;
}

int part_gpt_find_by_partuuid(const Disk *d, const char *uuid_str, Partition *out){
  if (!d || !uuid_str || !out) return -1;
  if (d->sector_size != 512) return -2;

  Guid want;
  int rc = guid_parse(uuid_str, &want);
  if (rc != 0) return -3;

  uint8_t sec[512];

  // GPT header at LBA1
  rc = disk_read((Disk*)d, 1, 1, sec);
  if (rc != 0) return -4;

  const GptHdr *h = (const GptHdr*)(const void*)sec;
  if (!(h->Sig[0]=='E' && h->Sig[1]=='F' && h->Sig[2]=='I' && h->Sig[3]==' ' &&
        h->Sig[4]=='P' && h->Sig[5]=='A' && h->Sig[6]=='R' && h->Sig[7]=='T')) {
    return -5; // not GPT
  }

  uint64_t pe_lba = rd64(&h->PartEntryLBA);
  uint32_t nent   = rd32(&h->NumPartEntries);
  uint32_t esz    = rd32(&h->SizeOfPartEntry);

  if (esz < sizeof(GptEntry)) return -6;
  if (nent == 0) return -7;
  if (esz > 512) return -8;

  uint32_t ents_per_sec = 512u / esz;
  if (ents_per_sec == 0) return -9;

  uint32_t sec_count = (nent + ents_per_sec - 1) / ents_per_sec;

  for (uint32_t si = 0; si < sec_count; si++){
    rc = disk_read((Disk*)d, pe_lba + si, 1, sec);
    if (rc != 0) return -10;

    for (uint32_t ei = 0; ei < ents_per_sec; ei++){
      uint32_t idx = si * ents_per_sec + ei;
      if (idx >= nent) break;

      const uint8_t *p = sec + ei * esz;
      const GptEntry *e = (const GptEntry*)(const void*)p;

      // unused entry => TypeGuid all zero
      const uint8_t *tg = (const uint8_t*)&e->TypeGuid;
      int all0 = 1;
      for (size_t k=0;k<sizeof(Guid);k++){ if (tg[k] != 0) { all0=0; break; } }
      if (all0) continue;

      if (!guid_eq(&e->PartGuid, &want)) continue;

      uint64_t first = rd64(&e->FirstLBA);
      uint64_t last  = rd64(&e->LastLBA);
      if (last < first) return -11;

      out->lba_start = first;
      out->lba_count = (last - first) + 1;
      out->type = 0; // GPT
      return 0;
    }
  }

  return -12; // not found
}