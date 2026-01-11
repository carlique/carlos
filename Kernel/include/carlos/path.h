#pragma once

static inline int path_is_sep(char c){ return c=='/' || c=='\\'; }

void path_normalize_abs(char *p, size_t cap);