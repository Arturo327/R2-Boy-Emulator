#ifndef CARTUCHO_H
#define CARTUCHO_H

#include "cartucho/mbc/none.h"
#include "cartucho/mbc/mbc1.h"
#include "cartucho/mbc/mbc2.h"
#include "cartucho/mbc/mbc3.h"
#include "cartucho/mbc/mbc5.h"
#include "cartucho/mbc/mbc6.h"
#include "cartucho/mbc/mbc7.h"
#include "cartucho/mbc/m161.h"
#include "cartucho/mbc/mmm01.h"

#include <stdint.h>

typedef struct GB GB;

typedef enum {
	MBC_NONE = 0,
	MBC1,
	MBC2,
	MBC3,
	MBC5,
	MBC6,
	MBC7,
	M161,
	MMM01
} MBC_Type;

typedef struct Cartucho {
	uint8_t *rom;
	uint32_t rom_size;
	uint32_t rom_banks;

	uint8_t *ram;
	uint32_t ram_size;
	uint32_t ram_banks;

	uint8_t mbc_mode;
	MBC_Type mbc_type;
	uint8_t rom_bank;
	uint8_t ram_bank;
	uint8_t ram_enabled;

	uint8_t bank1;
	uint8_t bank2;
	uint8_t multicart;
	uint8_t mbc30;
	uint8_t has_rtc;

	void *state;

	uint8_t has_rumble;
	uint8_t rumble_on;

	uint8_t battery;
	uint8_t save_needed;
	char title[17];

	uint8_t (*read_rom) (GB *gb, uint16_t addr);
	void (*write_rom) (GB *gb, uint16_t addr, uint8_t val);
	uint8_t (*read_ram) (GB *gb, uint16_t addr);
	void (*write_ram) (GB *gb, uint16_t addr, uint8_t val);
} Cartucho;

int load_rom (Cartucho *cart, const char *filename);

int load_sram (Cartucho *cart, const char *romfile);
int save_sram (Cartucho *cart, const char *romfile);
void save_sram_snapshot (Cartucho *cart, const char *romfile, const uint8_t *ram_snap,
			uint32_t ram_size, const RTC *rtc_snap);

void free_cart (Cartucho *cart);

#endif
