#ifndef MBC3_H
#define MBC3_H

#include <stdint.h>

typedef struct GB GB;

uint8_t mbc3_read_rom (GB *gb, uint16_t addr);
void mbc3_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc3_read_ram (GB *gb, uint16_t addr);
void mbc3_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
