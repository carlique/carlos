#pragma once
#include <stdint.h>

int  pci_init(void);      // parse MCFG, store ECAM base/range
void pci_list(void);      // enumerate + print

int pci_dump_bdf(uint8_t bus, uint8_t dev, uint8_t fun);

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off);
uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off);
uint8_t  pci_read8 (uint8_t bus, uint8_t dev, uint8_t fun, uint16_t off);

int pci_get_bus_range(uint8_t *start, uint8_t *end);