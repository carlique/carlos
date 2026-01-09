#pragma once
#include <stdint.h>

int  pci_init(void);      // parse MCFG, store ECAM base/range
void pci_list(void);      // enumerate + print

int pci_dump_bdf(uint8_t bus, uint8_t dev, uint8_t fun);