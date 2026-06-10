#ifndef CARTUCHO_H
#define CARTUCHO_H

#include <stdint.h>

typedef struct Cartucho {
	uint8_t *rom;
	uint32_t rom_size;

	uint8_t *ram;
	uint32_t ram_size;

	uint8_t mbc_mode;
	uint8_t mbc_type;
	uint8_t rom_bank;
	uint8_t ram_bank;
	uint8_t ram_enabled;

	int battery;
} Cartucho;

int load_rom (Cartucho *cart, char *filename);

#endif
