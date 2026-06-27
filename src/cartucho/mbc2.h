#ifndef MBC2_H
#define MBC2_H

#include <stdint.h>

typedef struct GB GB;

uint8_t mbc2_read_rom (GB *gb, uint16_t addr);
void mbc2_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc2_read_ram (GB *gb, uint16_t addr);
void mbc2_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
