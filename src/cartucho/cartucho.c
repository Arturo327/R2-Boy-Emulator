#include "cartucho/cartucho.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void normalize_mbc (Cartucho *cart, uint8_t header_type) {
	cart->battery = 0;

	switch (header_type) {
		case 0x00: cart->mbc_type = MBC_NONE; break;

		case 0x01: cart->mbc_type = MBC1; break;
		case 0x02: cart->mbc_type = MBC1; break;
		case 0x03: cart->mbc_type = MBC1; cart->battery = 1; break;

		case 0x05: cart->mbc_type = MBC2; break;
		case 0x06: cart->mbc_type = MBC2; cart->battery = 1; break;

		case 0x0F: cart->mbc_type = MBC3; cart->battery = 1; break;
		case 0x10: cart->mbc_type = MBC3; cart->battery = 1; break;
		case 0x11: cart->mbc_type = MBC3; break;
		case 0x12: cart->mbc_type = MBC3; break;
		case 0x13: cart->mbc_type = MBC3; cart->battery = 1; break;

		case 0x19: cart->mbc_type = MBC5; break;
		case 0x1A: cart->mbc_type = MBC5; break;
		case 0x1B: cart->mbc_type = MBC5; cart->battery = 1; break;
		case 0x1C: cart->mbc_type = MBC5; break;
		case 0x1D: cart->mbc_type = MBC5; break;
		case 0x1E: cart->mbc_type = MBC5; cart->battery = 1; break;

		default:
			fprintf(stderr, "Cartucho: Unknown MBC type (0x%02X), considering as ROM only\n", header_type);
			cart->mbc_type = MBC_NONE;
			break;
	}
}


int load_rom (Cartucho *cart, const char *filename) {
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
	fclose(f);
	if (a != (int)cart->rom_size) return 0;

	cart->rom_banks = cart->rom_size / 0x4000;

	normalize_mbc(cart, cart->rom[0x0147]);

	uint8_t ram_type = cart->rom[0x0149];
	uint32_t ram_sizes[] = {0, 2*1024, 8*1024, 32*1024, 128*1024, 64*1024};
	cart->ram_size = (ram_type < 6) ? ram_sizes[ram_type] : 0;
	cart->ram_banks = cart->ram_size / 0x2000;

	if (cart->ram_size) {
		cart->ram = calloc(1, cart->ram_size);
		if (cart->mbc_type == MBC_NONE) {
			cart->ram_enabled = 1;
		}
	}

	printf("ROM: %s\n", filename);
	return 1;
}

static void make_sav_path (const char *romfile, char *out, size_t outsize) {
	size_t len = strlen(romfile);
	const char *dot = strrchr(romfile, '.');
	const char *slash = strrchr(romfile, '/');

	size_t base_len = len;
	if (dot && (!slash || dot > slash)) {
		base_len = (size_t)(dot - romfile);
	}

	if (base_len > outsize - 5) base_len = outsize - 5;

	memcpy(out, romfile, base_len);
	memcpy(out + base_len, ".sav", 5);
}

int load_sram (Cartucho *cart, const char *romfile) {
	if (!cart->battery || !cart->ram || cart->ram_size == 0) return 0;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));

	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	int a = fread(cart->ram, 1, cart->ram_size, f);
	if (!a) {
		fprintf(stderr, "Could not load saved game %s\n", path);
		return 0;
	}
	fclose(f);

	printf("Loaded game: %s\n", path);
	return 1;
}

int save_sram (Cartucho *cart, const char *romfile) {
	if (!cart->battery || !cart->ram || cart->ram_size == 0) return 0;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Could not save game %s\n", path);
		return 0;
	}

	fwrite(cart->ram, 1, cart->ram_size, f);
	fclose(f);

	printf("Saved game: %s\n", path);
	return 1;
}
