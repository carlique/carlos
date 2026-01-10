#pragma once
#include <carlos/boot/bootinfo.h>
#include <carlos/fs.h>

void shell_run(const BootInfo *bi, Fs *fs); // fs may be NULL