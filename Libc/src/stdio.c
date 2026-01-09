#include <carlos/api.h>

extern const CarlosApi *carlos_api;

int puts(const char *s){
  size_t n = 0;
  while (s[n]) n++;
  carlos_api->write(s, n);
  carlos_api->write("\n", 1);
  return 0;
}