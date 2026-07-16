#ifndef GB_H
#define GB_H

#include <stdint.h>
#include <stdatomic.h>

#include "cpu/cpu.h"
#include "bus/bus.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "timer/timer.h"
#include "serial/serial.h"
#include "cartucho/cartucho.h"
#include "frontend/frontend.h"
#include "frontend/config.h"
#include "cartucho/save.h"

typedef struct Joypad {
	uint8_t buttons;
	uint8_t joyp;
	uint8_t kb_buttons;
	uint8_t pad_dpad;
	uint8_t pad_stick;
} Joypad;

typedef struct Memory {
	Cartucho cart;
	uint8_t bios[0x100];
	uint8_t vram[0x2000];
	uint8_t wram[0x2000];
	uint8_t oam[0xA0];
	uint8_t hram[0x7F];
} Memory;

typedef struct Config {
	Keymap keymap;
	uint8_t turbo;
	_Atomic int volume;
	_Atomic uint8_t muted;
	DmgPalette palette;
} Config;

typedef struct DMA {
	uint8_t active;
	uint16_t src;
	uint8_t index;
	uint8_t delay;
} DMA;

typedef struct GB {
	OpcodeTable opcodes;

	CPU cpu;
	Bus bus;

	LCD lcd;
	Audio audio;
	Gamepad pad;
	Link link;

	Memory memory;
	PPU ppu;
	APU apu;
	Interrupts interrupts;
	Timer timer;
	Joypad joypad;
	Serial serial;
	DMA dma;

	uint8_t boot_rom_enabled;
	uint8_t boot_rom_disable_pending;

	uint64_t clock;
	int running;

	Config cfg;
	AutoSave save;
} GB;

void init (GB *gb, const char *romfile, const char *biosfile);
void init_test (GB *gb, const char *romfile);

void cleanup (GB *gb, const char *romfile);
void cleanup_core (GB *gb, const char *romfile);

void gb_step (GB *gb);

#endif
