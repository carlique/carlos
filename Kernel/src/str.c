#include <stddef.h>
#include <carlos/str.h>

size_t kstrlen(const char *s){
  size_t n = 0;
  while (s && s[n]) n++;
  return n;
}

int kstreq(const char *a, const char *b){
  if (!a || !b) return 0;
  while (*a && *b){
    if (*a != *b) return 0;
    a++; b++;
  }
  return *a == 0 && *b == 0;
}

char *kstrncpy(char *dst, const char *src, size_t n){
  if (!dst || n == 0) return dst;
  size_t i = 0;
  if (src) {
    for (; i < n && src[i]; i++) dst[i] = src[i];
  }
  for (; i < n; i++) dst[i] = 0;
  return dst;
}