#ifndef MMM01_H
#define MMM01_H

#include <stdint.h>
#include <time.h>

typedef struct GB GB;
typedef struct Cartucho Cartucho;

typedef struct MMM01Regs {
	uint8_t mapped;
	uint8_t multiplex;
	uint8_t mbc1_mode;
	uint8_t mode_write_disable;

	uint8_t rom_bank_low;
	uint8_t rom_bank_mid;
	uint8_t rom_bank_high;
	uint8_t rom_bank_mask;

	uint8_t ram_bank_low;
	uint8_t ram_bank_high;
	uint8_t ram_bank_mask;
} MMM01Regs;

int mmm01_init (Cartucho *cart);
void mmm01_free (Cartucho *cart);

uint8_t mmm01_read_rom (GB *gb, uint16_t addr);
void mmm01_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mmm01_read_ram (GB *gb, uint16_t addr);
void mmm01_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
