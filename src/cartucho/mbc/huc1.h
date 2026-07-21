#ifndef HUC1_H
#define HUC1_H

#include <stdint.h>

typedef struct GB GB;

uint8_t huc1_read_rom (GB *gb, uint16_t addr);
void huc1_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t huc1_read_ram (GB *gb, uint16_t addr);
void huc1_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
