#ifndef GB_H
#define GB_H

#include <stdint.h>
#include "cpu/cpu.h"
#include "bus/bus.h"
#include "ppu/ppu.h"
#include "timer/timer.h"
#include "cartucho/cartucho.h"
#include "cpu/opcodes.h"
#include "frontend/frontend.h"

typedef struct Joypad {
	uint8_t buttons;
	uint8_t joyp;
	uint8_t SB;
	uint8_t SC;
} Joypad;

typedef struct Memory {
	Cartucho cart;
	uint8_t bios[0x100];
	uint8_t vram[0x2000];
	uint8_t wram[0x2000];
	uint8_t oam[0xA0];
	uint8_t hram[0x7F];
} Memory;

typedef struct GB {
	OpcodeTable opcodes;

	CPU cpu;
	Bus bus;

	LCD lcd;
	Memory memory;
	PPU ppu;
	Interrupts interrupts;
	Timer timer;
	Joypad joypad;

	uint8_t dma_active;
	uint16_t dma_src;
	uint8_t dma_index;

	uint8_t boot_rom_enabled;
	uint8_t boot_rom_disable_pending;

	uint64_t clock;
	int running;
} GB;

void init (GB *gb, const char *romfile, const char *biosfile);
void init_test (GB *gb, const char *romfile, const char *biosfile);

void cleanup (GB *gb);
void gb_step (GB *gb);

#endif
