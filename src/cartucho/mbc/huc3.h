#ifndef HUC3_H
#define HUC3_H

#include <stdint.h>
#include <time.h>

typedef struct GB GB;
typedef struct Cartucho Cartucho;

typedef struct HuC3State {
	uint8_t select;	 
			 
	uint8_t cmd;
	uint8_t arg;
	uint8_t addr;	 
	uint8_t response;

	uint8_t ready;
	uint8_t ir;

	uint8_t mem[256];
	int64_t base;
} HuC3State;

int huc3_init (Cartucho *cart);
void huc3_free (Cartucho *cart);

uint8_t huc3_read_rom (GB *gb, uint16_t addr);
void huc3_write_rom (GB *gb, uint16_t addr, uint8_t val);
uint8_t huc3_read_ram (GB *gb, uint16_t addr);
void huc3_write_ram (GB *gb, uint16_t addr, uint8_t val);

#endif
