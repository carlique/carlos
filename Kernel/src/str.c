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