#pragma once
#include <stddef.h>
#include <stdint.h>

size_t kstrlen(const char *s);
int    kstreq(const char *a, const char *b);
char *kstrncpy(char *dst, const char *src, size_t n);