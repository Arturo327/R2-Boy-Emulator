#ifndef MBC5_H
#define MBC5_H

#include <stdint.h>

typedef struct GB GB;

uint8_t mbc5_read_rom (GB *gb, uint16_t addr);
void mbc5_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc5_read_ram (GB *gb, uint16_t addr);
void mbc5_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
