#pragma once
#include <carlos/fs.h>

int mkdir_cmd(Fs *fs, const char *arg);              // shell-style
int mkdir_main(Fs *fs, int argc, char **argv);       // future exec-style