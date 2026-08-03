#ifndef GB_H
#define GB_H

#include <stdint.h>
#include <stdatomic.h>

#include "model.h"
#include "cpu/cpu.h"
#include "bus/bus.h"
#include "dma/dma.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "timer/timer.h"
#include "serial/serial.h"
#include "cartucho/cartucho.h"
#include "cartucho/save.h"
#include "cartucho/savestate.h"
#include "frontend/frontend.h"
#include "frontend/config.h"

#define TILT_RIGHT 0x01
#define TILT_LEFT 0x02
#define TILT_UP 0x04
#define TILT_DOWN 0x08

#define TICKS_PER_FRAME 70224

typedef struct Joypad {
	uint8_t buttons;
	uint8_t joyp;
	uint8_t kb_buttons;
	uint8_t pad_dpad;
	uint8_t pad_stick;

	uint8_t kb_tilt;
	int16_t pad_tilt_x;
	int16_t pad_tilt_y;
} Joypad;

typedef struct Memory {
	Cartucho cart;
	uint8_t *bios;
	uint8_t *vram;
	uint8_t *wram;
	uint8_t oam[0xA0];
	uint8_t hram[0x7F];
	// only CGB
	uint8_t vram_bank, wram_bank;
	uint8_t ff72, ff73, ff74, ff75;
	uint8_t key0, key1;
	uint8_t bg_palette_ram[64];
	uint8_t obj_palette_ram[64];
} Memory;

typedef struct GB {
	OpcodeTable opcodes;

	CPU cpu;
	Bus bus;

	LCD lcd;
	Audio audio;
	Gamepad pad;
	Link link;
	Printer printer;
	Webcam webcam;

	Memory memory;
	PPU ppu;
	APU apu;
	Interrupts interrupts;
	Timer timer;
	Joypad joypad;
	Serial serial;
	DMA dma;
	HDMA hdma;

	uint8_t boot_rom_enabled;
	uint8_t boot_rom_disable_pending;
	uint8_t hay_bios;

	uint64_t clock;
	uint8_t running;
	uint8_t on;
	uint8_t paused;

	const char *romfile;
	uint8_t state_save_pending;
	uint8_t state_load_pending;
	uint8_t state_num;

	Model model;
	Config cfg;
	AutoSave save;
} GB;

void reset_gb (GB *gb);
void init (GB *gb, const char *romfile, const char *biosfile, Model model);
void init_test (GB *gb, const char *romfile, Model model);

void cleanup (GB *gb);
void cleanup_core (GB *gb);

void gb_step (GB *gb);

#endif
