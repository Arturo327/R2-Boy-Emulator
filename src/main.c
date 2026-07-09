#include "gb.h"

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <SDL2/SDL.h>

static int check_regs (GB *gb, const char *romfile)
{
	uint8_t b=gb->cpu.b, c=gb->cpu.c, d=gb->cpu.d, e=gb->cpu.e, h=gb->cpu.h, l=gb->cpu.l;

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
	return 1;
}

static int run_test (const char *romfile)
{
	GB gb = {0};

	init_test (&gb, romfile, "no_load_bios");
	if (!gb.running) {
		fprintf(stderr, "FAIL: %s (could not load ROM)\n", romfile);
		return 1;
	}

	const uint64_t MAX_CYCLES = 500000000ULL;
	int truth_time = 0;
	while (gb.clock < MAX_CYCLES) {
		if (gb.cpu.instr_head >= gb.cpu.instr_tail && !gb.cpu.halted &&
				gb.bus.read8(gb.bus.ctx, gb.cpu.pc) == 0x40) {
			truth_time = 1;
			break;
		}
		gb_step(&gb);
	}

	if (!truth_time) {
		printf("TIMEOUT: %s\n", romfile);
		cleanup(&gb);
		return 1;
	}

	int result = check_regs(&gb, romfile);
	cleanup(&gb);
	return result;
}

static void sleep_until (uint64_t target, uint64_t freq) {
	while (1) {
		uint64_t now = SDL_GetPerformanceCounter();
		if (now >= target) break;
		uint64_t remaining_ms = (target - now) * 1000 / freq;
		if (remaining_ms > 2) SDL_Delay(1);
	}
}

typedef struct {
	char *romfile;
	char *biosfile;
	int debug;
} Args;

static inline void print_usage_err (const char *prog) {
	fprintf(stderr, "Use: %s [-h|--help] [-v|--version] [-d|--debug] [-b|--bios bios_file] game.gb\nExecute %s --help for more info\n", prog, prog);
}
static inline void print_usage (const char *prog) {
	printf("R2-Boy v1.0.0-beta\n\n");
	printf("USE:\n");
	printf("    %s [-h|--help] [-v|--version] [-d|--debug] [-b|--bios bios_file] game.gb\n\n", prog);

	printf("OPTIONS:\n");
	printf("    -h, --help              Print this help message\n");
    	printf("    -v, --version           Print version information\n");
    	printf("    -d, --debug             Desactivates frontend and checks registers to determine weather the test passed\n");
    	printf("    -b, --bios <BIOS_FILE>  Specify the path of the BOOT ROM. [default: 'roms/boot.bin'] If it is not found, the emulator will boot without a BIOS.\n\n");

    	printf("KEYBOARD MAPPINGS:\n");
    	printf("    Directional Arrows      D-Pad (Up, Down, Left, Right)\n");
    	printf("    X                       A Button\n");
    	printf("    Z                       B Button\n");
    	printf("    Enter                   START\n");
    	printf("    Backspace               SELECT\n");
}
static inline void print_version (void) {
	printf("R2-Boy v1.0.0-beta\n");
}

static Args parse_args (int argc, char *argv[])
{
	static struct option long_options[] = {
		{"bios", required_argument, 0, 'b'},
		{"debug", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	Args args = {
		.biosfile = "roms/bios.bin",
		.debug = 0,
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "hvdb:", long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			printf("DEBUG MODE active\n");
			args.debug = 1;
			break;
		case 'b':
			printf("BIOS: %s\n", optarg);
			args.biosfile = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			exit(0);
		case 'v':
			print_version();
			exit(0);
		default:
			print_usage_err(argv[0]);
			exit(1);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No ROM found\n");
		print_usage(argv[0]);
		exit(1);
	}
	args.romfile = argv[optind];
	return args;
}

static int init_emulator (GB *gb, const char *romfile, const char *biosfile) {
	init(gb, romfile, biosfile);
	if (!gb->running) {
		cleanup(gb);
		return 0;
	}
	return 1;
}

static int run_frame (GB *gb, uint32_t max_queued)
{
	while (gb->clock < 70224) {
		gb_step(gb);

		if (gb->apu.buffer_pos >= 256) {
			queue_audio(gb);
			while (ring_used(&gb->audio.ring) > max_queued * 2)
				SDL_Delay(1);
		}

		if ((gb->clock & 511) == 0)
			if (!handle_events(gb)) return 0;
	}
	return 1;
}

static void sync_frame (uint64_t *next_frame, uint64_t frame_ticks, uint64_t freq)
{
	*next_frame += frame_ticks;
	uint64_t now = SDL_GetPerformanceCounter();
	if (*next_frame < now) *next_frame = now;
	sleep_until(*next_frame, freq);
}

static void run (GB *gb)
{
	double frames_per_sec = 4194304.0 / 70224.0;
	uint32_t samples_per_frame = (uint32_t)((double)gb->audio.sample_rate / frames_per_sec * 2.0);
	uint32_t max_queued = (uint32_t)(samples_per_frame * 1.5);

	uint64_t freq = SDL_GetPerformanceFrequency();
	uint64_t frame_ticks = (uint64_t)(freq / frames_per_sec);
	uint64_t next_frame = SDL_GetPerformanceCounter();

	while (gb->running) {

		if (!run_frame(gb, max_queued)) {
			gb->running = 0;
			break;
		}

		queue_audio(gb);
		update_screen(&gb->lcd, gb->ppu.framebuffer, gb->memory.cart.rumble_on);
		update_rumble(&gb->pad, gb->memory.cart.rumble_on);
		gb->clock -= 70224;

		while (ring_used(&gb->audio.ring) > max_queued)
			SDL_Delay(1);

		sync_frame(&next_frame, frame_ticks, freq);
	}

	cleanup(gb);
}

int main (int argc, char *argv[])
{
	Args args = parse_args(argc, argv);

	if (args.debug)
		return run_test(args.romfile);

	GB *gb = malloc(sizeof(GB));
	if (!gb) {
		fprintf(stderr, "FAIL: Not enough memory. Buy more RAM.\n");
		return 1;
	}

	if (!init_emulator(gb, args.romfile, args.biosfile))
		return 1;

	run(gb);

	free(gb);
	gb = NULL;
	return 0;
}
