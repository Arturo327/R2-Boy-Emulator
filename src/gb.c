#include "gb.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

void init (GB *gb, char *romfile) {
	memset(gb, 0, sizeof(GB));

	init_cpu(&gb->cpu);
	init_ppu(&gb->ppu);
	init_bus(&gb->bus, gb);
	init_opcodes(&gb->opcodes);

	if (!init_screen(&gb->lcd)) {
		gb->running = 0;
		return;
	}

	if (!load_rom(&gb->memory.cart, romfile)) {
		fprintf(stderr, "Failed to load ROM: %s\n", romfile);
		gb->running = 0;
		return;
	}

	gb->joypad.joyp = 0xCF;

	gb->running = 1;
}

void cleanup (GB *gb) {
    	cleanup_screen(&gb->lcd);
    	(void) gb;
}

void gb_step (GB *gb) {
	int cycles;

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





















