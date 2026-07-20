#ifndef MBC7_H
#define MBC7_H

#include <stdint.h>

typedef struct GB GB;
typedef struct Cartucho Cartucho;

enum {
	EE_IDLE = 0,
	EE_READING,
	EE_WRITING
};

typedef struct MBC7State {
	uint8_t ram_enable1, ram_enable2;
	uint16_t latch_x, latch_y;
	uint8_t latched;
	uint8_t eeprom_enabled;
	uint8_t cs, clk, di, do_bit;
	uint8_t start_seen;
	uint16_t shift_reg;
	uint8_t bit_count;
	uint8_t op_addr;
	uint8_t write_all;
	uint8_t ee_state;
} MBC7State;

int mbc7_init (Cartucho *cart);
void mbc7_free (Cartucho *cart);

uint8_t mbc7_read_rom (GB *gb, uint16_t addr);
void mbc7_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc7_read_ram (GB *gb, uint16_t addr);
void mbc7_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
