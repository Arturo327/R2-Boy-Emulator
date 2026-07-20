#ifndef NONE_H
#define NONE_H

#include <stdint.h>

typedef struct GB GB;

uint8_t mbcNone_read_rom (GB *gb, uint16_t addr);
void mbcNone_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbcNone_read_ram (GB *gb, uint16_t addr);
void mbcNone_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
