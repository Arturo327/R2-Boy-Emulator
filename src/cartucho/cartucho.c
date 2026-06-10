#include "cartucho/cartucho.h"
#include <stdio.h>
#include <stdlib.h>

int load_rom (Cartucho *cart, char *filename) {
	cart->rom_bank = 1;
	cart->ram_bank = 0;
	cart->ram_enabled = 0;
	cart->mbc_mode = 0;

	FILE *f = fopen(filename, "rb");
	if (!f) {
		return 0;
	}

	fseek(f, 0, SEEK_END);
	cart->rom_size = ftell(f);
	rewind(f);

	cart->rom = malloc(cart->rom_size);

	if (!cart->rom) {
		fclose(f);
		return 0;
	}

	int a = fread(cart->rom, 1, cart->rom_size, f);
	if (a != (int)cart->rom_size) return 0;

	cart->mbc_type = cart->rom[0x0147];

	uint8_t ram_type = cart->rom[0x0149];
	uint32_t ram_sizes[] = {0, 0, 8*1024, 32*1024, 128*1024, 64*1024};
	if (ram_type < 6) cart->ram_size = ram_sizes[ram_type];
	if (cart->ram_size) {
		cart->ram = calloc(1, cart->ram_size);
	}

	fclose(f);
	return 1;
}
