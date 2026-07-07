#ifndef CARTUCHO_H
#define CARTUCHO_H

#include <stdint.h>

#define MBC_NONE 0
#define MBC1 1
#define MBC2 2
#define MBC3 3
#define MBC5 5

typedef struct GB GB;

typedef struct Cartucho {
	uint8_t *rom;
	uint32_t rom_size;
	uint32_t rom_banks;

	uint8_t *ram;
	uint32_t ram_size;
	uint32_t ram_banks;

	uint8_t mbc_mode;
	uint8_t mbc_type;
	uint8_t rom_bank;
	uint8_t ram_bank;
	uint8_t ram_enabled;

	uint8_t bank1;
	uint8_t bank2;
	uint8_t multicart;

	int battery;

	uint8_t (*read_rom) (GB *gb, uint16_t addr);
	void (*write_rom) (GB *gb, uint16_t addr, uint8_t val);
	uint8_t (*read_ram) (GB *gb, uint16_t addr);
	void (*write_ram) (GB *gb, uint16_t addr, uint8_t val);
} Cartucho;

int load_rom (Cartucho *cart, const char *filename);
int load_sram (Cartucho *cart, const char *romfile);
int save_sram (Cartucho *cart, const char *romfile);

#endif
