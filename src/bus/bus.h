#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "cpu/opcodes.h"

typedef struct GB GB;

typedef struct Interrupts {
	uint8_t IE;
	uint8_t IF;
	uint8_t IME;
	uint8_t ei_pending;
} Interrupts;

typedef struct Bus {
    	OpcodeTable *opcodes;
	Interrupts *interrupts;

    	uint8_t (*read8) (void *ctx, uint16_t addr);
    	void (*write8) (void *ctx, uint16_t addr, uint8_t val);

	void *ctx;
} Bus;

void init_bus (Bus *bus, GB *gb);
void joypad_update (GB *gb, uint8_t new_buttons);
void oam_bug (GB *gb, uint16_t val, int is_write);
uint8_t bus_read8 (void *ctx, uint16_t addr);
void bus_write8 (void *ctx, uint16_t addr, uint8_t val);

#endif
