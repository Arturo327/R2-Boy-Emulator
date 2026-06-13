#include "gb.h"

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <SDL2/SDL.h>

static int run_test (const char *romfile) {

	GB gb = {0};

	init_test (&gb, romfile, "no_load_bios");
	if (!gb.running) {
		printf("FAIL: %s (could not load ROM)\n", romfile);
		return 1;
	}

	const int MAX_CYCLES = 50000000;
	int truth_time = 0;
	while (gb.clock < MAX_CYCLES) {
		if (gb.bus.read8(gb.bus.ctx, gb.cpu.pc) == 0x40) {
			truth_time = 1;
			break;
		}
		gb_step(&gb);
	}

	if (!truth_time) {
		printf("TIMEOUT: %s\n", romfile);
		return 1;
	}

	uint8_t b=gb.cpu.b, c=gb.cpu.c, d=gb.cpu.d, e=gb.cpu.e, h=gb.cpu.h, l=gb.cpu.l;
	
	if (b==3 && c==5 && d==8 && e==13 && h==21 && l==34) {
		printf("PASS: %s\n", romfile);
		return 0;
	}

	if (b==0x42 && c==0x42 && d==0x42 && e==0x42 && h==0x42 && l==0x42) {
		printf("FAIL: %s (test reported fail, B-L = 0x42)\n", romfile);
		return 1;
	}

	printf("FAIL (unexpected pattern, revise execution): %s (B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X)\n",
		romfile, b, c, d, e, h, l);
	return 0;
}

int main (int argc, char *argv[])
{
	static struct option long_options[] = {
		{"bios",  required_argument, 0, 'b'},
		{"debug", no_argument, 0, 'd'},
		{0, 0, 0, 0}
	};

	char *romfile;
	char default_bios[] = "roms/bios.bin";
	char *biosfile = default_bios;
	int debug = 0;

	int opt;
	while ((opt = getopt_long(argc, argv, "db:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			printf("DEBUG MODE active\n");
			debug = 1;
			break;

		case 'b':
			printf("BIOS: %s\n", optarg);
			biosfile = optarg;
			break;

		default:
			fprintf(stderr, "Use: %s [-d/--debug] [-b/--bios bios_file] game.gb \n", argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No ROM founded\n");
		return 1;
	}
	romfile = argv[optind];

	if (debug) return run_test(romfile);

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
