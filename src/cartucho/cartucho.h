#ifndef CARTUCHO_H
#define CARTUCHO_H

#include <stdint.h>
#include <time.h>

#define MBC_NONE 0
#define MBC1 1
#define MBC2 2
#define MBC3 3
#define MBC5 5

typedef struct GB GB;

typedef struct RTC {
	uint8_t s, m, h;
	uint16_t d;
	uint8_t halt;
	uint8_t carry;

	uint8_t s_l, m_l, h_l;
	uint16_t d_l;
	uint8_t halt_l;
	uint8_t carry_l;

	uint8_t latch_prev;
	time_t base;
} RTC;

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
	uint8_t mbc30;

	uint8_t has_rtc;
	RTC rtc;

	uint8_t has_rumble;

	uint8_t battery;

	uint8_t (*read_rom) (GB *gb, uint16_t addr);
	void (*write_rom) (GB *gb, uint16_t addr, uint8_t val);
	uint8_t (*read_ram) (GB *gb, uint16_t addr);
	void (*write_ram) (GB *gb, uint16_t addr, uint8_t val);
} Cartucho;

int load_rom (Cartucho *cart, const char *filename);
int load_sram (Cartucho *cart, const char *romfile);
int save_sram (Cartucho *cart, const char *romfile);

#endif
