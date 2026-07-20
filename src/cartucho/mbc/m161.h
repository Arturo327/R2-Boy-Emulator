#ifndef M161_H
#define M161_H

#include <stdint.h>

typedef struct GB GB;

uint8_t m161_read_rom (GB *gb, uint16_t addr);
void m161_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t m161_read_ram (GB *gb, uint16_t addr);
void m161_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
