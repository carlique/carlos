#include <stddef.h>
#include <carlos/path.h>

// in-place normalize: collapse //, resolve /./ and /../
void path_normalize_abs(char *p, size_t cap){
  if (!p || cap == 0) return;

  // Ensure there is a NUL within cap so we never scan out of bounds
  size_t in_len = 0;
  while (in_len < cap && p[in_len]) in_len++;
  if (in_len == cap) { p[cap-1] = 0; in_len = cap-1; }

  // Use a bounded temp buffer
  char out[256];
  size_t out_cap = cap;
  if (out_cap > sizeof(out)) out_cap = sizeof(out);
  if (out_cap == 0) return;

  size_t oi = 0;

  // Always start absolute
  out[oi++] = '/';
  out[oi] = 0;

  // Skip leading separators in input
  size_t i = 0;
  while (i < in_len && path_is_sep(p[i])) i++;

  while (i < in_len && p[i]) {
    while (i < in_len && path_is_sep(p[i])) i++;
    if (i >= in_len || !p[i]) break;

    // read component
    char comp[64];
    size_t ci = 0;
    while (i < in_len && p[i] && !path_is_sep(p[i]) && ci + 1 < sizeof(comp)) {
      char c = p[i++];
      comp[ci++] = (c == '\\') ? '/' : c;
    }
    comp[ci] = 0;

    if (ci == 0) break;

    // "." -> skip
    if (ci == 1 && comp[0] == '.') continue;

    // ".." -> pop
    if (ci == 2 && comp[0] == '.' && comp[1] == '.') {
      if (oi > 1) {
        if (oi > 1 && out[oi-1] == '/') oi--;
        while (oi > 1 && out[oi-1] != '/') oi--;
        out[oi] = 0;
      }
      continue;
    }

    // append "/comp"
    if (oi > 1 && out[oi-1] != '/' && oi + 1 < out_cap) out[oi++] = '/';
    for (size_t k = 0; comp[k] && oi + 1 < out_cap; k++) out[oi++] = comp[k];
    out[oi] = 0;

    // skip rest of too-long component if we truncated comp[]
    while (i < in_len && p[i] && !path_is_sep(p[i])) i++;
  }

  // trim trailing slash except root
  if (oi > 1 && out[oi-1] == '/') out[--oi] = 0;

  // copy back bounded by cap
  size_t copy_n = oi;
  if (copy_n >= cap) copy_n = cap - 1;
  for (size_t k = 0; k < copy_n; k++) p[k] = out[k];
  p[copy_n] = 0;
}