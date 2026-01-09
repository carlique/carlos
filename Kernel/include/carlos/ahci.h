#pragma once
#include <stdint.h>

int ahci_probe(void);                 // auto-find first AHCI controller
int ahci_probe_bdf(uint8_t b, uint8_t d, uint8_t f); // explicit

// Read `count` sectors (512B each) from `lba` into `buf` using port `port`.
int ahci_read(uint32_t port, uint64_t lba, uint32_t count, void *buf);