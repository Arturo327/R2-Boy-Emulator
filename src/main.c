#include "gb.h"
#include <stdio.h>
#include <SDL2/SDL.h>

int main (int argc, char *argv[]) 
{
	const char *romfile;

	if (argc > 1) {
        	romfile = argv[1];
        	printf("ROM: %s\n", romfile);
    	} else {
		fprintf(stderr, "No ROM founded\n");
		return 1;
	}

	const char biosfile[] = "roms/bios.bin";

	GB gb = {0};

	init(&gb, romfile, biosfile);
	if (!gb.running) {
		cleanup(&gb);
		return 1;
	}

	const double FRAME_MS = 70224.0 * 1000.0 / 4194304.0;
	while (gb.running) {
		if (!handle_events(&gb)) {
			gb.running = 0;
			break;
		}

		uint64_t start = SDL_GetPerformanceCounter();

		while (gb.clock < 70224) {
			gb_step(&gb);
		}
		update_screen(&gb.lcd, gb.ppu.framebuffer);
		gb.clock -= 70224;

		uint64_t end = SDL_GetPerformanceCounter();
		double elapsed = (end - start) * 1000.0 / SDL_GetPerformanceFrequency();
		double delay = FRAME_MS - elapsed;
		if (delay > 0) SDL_Delay((uint32_t)delay);
	}

	cleanup(&gb);
	return 0;
}
