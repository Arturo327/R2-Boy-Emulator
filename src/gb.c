#include "gb.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static int load_bios (GB *gb, const char *filename) {
	FILE *f = fopen(filename, "rb");
	if (!f) return 0;

	size_t n = fread(gb->memory.bios, 1, 0x100, f);
	fclose(f);

	return n == 0x100;
}

static int init_core (GB *gb, const char *romfile, const char *biosfile) {
	memset(gb, 0, sizeof(GB));

	init_ppu(&gb->ppu);
	init_opcodes(&gb->opcodes);

	if (!load_bios(gb, biosfile)) {

		init_ppu_reg(&gb->ppu);
		init_cpu(&gb->cpu);
		gb->boot_rom_enabled = 0;
		printf("Could not load BOOT ROM %s. Running whithout BIOS\n", biosfile);

	} else {

		printf("Using BOOT ROM %s\n", biosfile);
		gb->boot_rom_enabled = 1;
	}

	init_bus(&gb->bus, gb);

	strncpy(gb->rom_path, romfile, sizeof(gb->rom_path) - 1);
	gb->rom_path[sizeof(gb->rom_path) - 1] = '\0';

	if (!load_rom(&gb->memory.cart, romfile)) {
		fprintf(stderr, "Failed to load ROM: %s\n", romfile);
		return 1;
	}

	load_sram(&gb->memory.cart, romfile);

	gb->joypad.joyp = 0xCF;

	return 0;
}

void init (GB *gb, const char *romfile, const char *biosfile) {

	if (init_core(gb, romfile, biosfile)) {
		gb->running = 0;
		return;
	}

	if (!init_screen(&gb->lcd)) {
		gb->running = 0;
		return;
	}

	gb->running = 1;
}

void init_test (GB *gb, const char *romfile, const char *biosfile) {

	if (init_core(gb, romfile, biosfile)) {
		gb->running = 0;
		return;
	}

	gb->running = 1;
}

void cleanup (GB *gb) {
	save_sram(&gb->memory.cart, gb->rom_path);
	cleanup_screen(&gb->lcd);
	(void) gb;
}

void gb_step (GB *gb) {
	int cycles;

	if (gb->boot_rom_disable_pending && gb->cpu.pc >= 0x0100) {
		gb->boot_rom_enabled = 0;
		gb->boot_rom_disable_pending = 0;
	}

	if (gb->dma_active) {
		gb->memory.oam[gb->dma_index] = gb->bus.read8(gb->bus.ctx, gb->dma_src + gb->dma_index);
		gb->dma_index++;
		cycles = 4;
		if (gb->dma_index >= 0xA0)
			gb->dma_active = 0;
	} else {
		cycles = cpu_step(&gb->cpu);
	}
	
	if (timer_step(&gb->timer, cycles)) {
		gb->interrupts.IF |= 0x04;
	}
	ppu_step(&gb->ppu, cycles);
	gb->clock += cycles;

}
