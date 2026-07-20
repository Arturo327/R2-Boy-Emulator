#ifndef MBC3_H
#define MBC3_H

#include <stdint.h>
#include <time.h>

typedef struct GB GB;
typedef struct Cartucho Cartucho;

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

int rtc_init (Cartucho *cart);
void rtc_free (Cartucho *cart);

uint8_t mbc3_read_rom (GB *gb, uint16_t addr);
void mbc3_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t mbc3_read_ram (GB *gb, uint16_t addr);
void mbc3_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
