#pragma once
#include <carlos/fs.h>

// Linux-ish shape: argv[0] is "ls".
// Returns 0 on success, <0 on error.
int ls_cmd(Fs *fs, const char *arg);              // shell-style: one arg (or NULL)
int ls_main(Fs *fs, int argc, char **argv);       // future exec-style