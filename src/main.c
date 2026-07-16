#include "gb.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <SDL2/SDL.h>

#define GB_CLOCK_HZ 4194304ULL
#define TICKS_PER_FRAME 70224
#define AUTOSAVE_RATE_FRAMES ((uint32_t)(AUTOSAVE_INTERVAL_MS / 1000.0 * (((double)GB_CLOCK_HZ) / ((double)TICKS_PER_FRAME))))

#define VERSION "R2-Boy v1.0.0-beta"

static volatile sig_atomic_t quit_requested = 0;

static void on_signal (int sig) {
	(void)sig;
	quit_requested = 1;
}

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
	GB *gb = malloc(sizeof(GB));
	if (gb == NULL) {
		fprintf(stderr, "FAIL: Not enough memory. Buy more RAM.\n");
		return 1;
	}

	init_test (gb, romfile);
	if (!gb->running) {
		fprintf(stderr, "FAIL: %s (could not load ROM)\n", romfile);
		cleanup_core(gb, romfile);
		free(gb);
		return 1;
	}

	const uint64_t MAX_CYCLES = 500000000ULL;
	int truth_time = 0;
	while (gb->clock < MAX_CYCLES) {
		if (gb->cpu.instr_head >= gb->cpu.instr_tail && !gb->cpu.halted &&
				gb->bus.read8(gb->bus.ctx, gb->cpu.pc) == 0x40) {
			truth_time = 1;
			break;
		}
		gb_step(gb);
	}

	if (!truth_time) {
		printf("TIMEOUT: %s\n", romfile);
		cleanup_core(gb, romfile);
		free(gb);
		return 1;
	}

	int result = check_regs(gb, romfile);
	cleanup_core(gb, romfile);
	free(gb);
	return result;
}

typedef struct {
	char *romfile;
	char *biosfile;
	int debug;
	int link_host_port;
	char *link_connect_addr;

	int volume;
	int volume_set;
	int mute;
	char *palette;
} Args;

static inline void print_err (const char *prog)
{
	fprintf(stderr, "Not a valid command\nExecute %s --help for more info\n", prog);
}

static inline void print_usage (const char *prog)
{
	printf("%s\n", VERSION);
	printf("Nintendo Game Boy (DMG) Emulator\n\n");

	printf("USAGE:\n");
	printf("    %s [OPTIONS] <game.gb>\n\n", prog);

	printf("OPTIONS:\n");
	printf("    -h, --help\n");
	printf("	Display this help message and exit.\n\n");

	printf("    -v, --version\n");
	printf("	Display version information and exit.\n\n");

	printf("    -d, --debug\n");
	printf("	Run in headless mode and verify CPU registers after execution.\n");
	printf("	Intended for automated test ROMs.\n\n");

	printf("    -b, --bios <BIOS_FILE>\n");
	printf("	Use the specified Game Boy Boot ROM.\n");
	printf("	Default: roms/boot.bin\n");
	printf("	If the file cannot be loaded, the emulator boots without a Boot ROM.\n\n");

	printf("    --link-host <PORT>\n");
	printf("	Host a Game Link session and listen for an incoming connection\n");
	printf("	on the specified TCP port.\n\n");

	printf("    --link-connect <IP:PORT>\n");
	printf("	Connect to a remote Game Link host using the specified\n");
	printf("	IP address and TCP port.\n\n");

	printf("    --volume <0..100>\n");
	printf("	Set the audio output volume. Default: 100.\n\n");

	printf("    --mute\n");
	printf("	Start with audio muted.\n\n");

	printf("    --palette <NAME>\n");
	printf("	Select a built-in color palette. NAME is one of:\n");
	printf("	\"DMG\", \"pocket\", \"BGB\", \"choco\", \"pocket_green\"\n\n");

	printf("    --remap\n");
	printf("	Run an interactive key-remap prompt at startup.\n");
	printf("	The mapping is persisted to ~/.config/r2boy/config.ini\n\n");

	printf("DEFAULT KEYBOARD CONTROLS:\n");
	printf("    Arrow Keys		    D-Pad\n");
	printf("    X			    A\n");
	printf("    Z			    B\n");
	printf("    Enter		    START\n");
	printf("    Backspace		    SELECT\n");
	printf("    Tab			    Turbo (hold)\n");
	printf("    M / + / -		    Mute / Vol+ / Vol-\n");
	printf("    P			    Cycle color palette\n");
}

static inline void print_version (void) {
	printf("%s\n", VERSION);
}

static int parse_port (const char *str, uint16_t *out)
{
	if (!str || !*str) return 0;

	errno = 0;
	char *end;
	long val = strtol(str, &end, 10);

	if (errno == ERANGE || *end != '\0') return 0;
	if (val < 1 || val > 65535) return 0;

	*out = (uint16_t) val;
	return 1;
}


static Args parse_args (int argc, char *argv[])
{
	static struct option long_options[] =
	{
		{"bios", required_argument, 0, 'b'},
		{"debug", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"link-host", required_argument, 0, 'H'},
		{"link-connect", required_argument, 0, 'C'},
		{"volume", required_argument, 0, 'V'},
		{"mute", no_argument, 0, 'M'},
		{"palette", required_argument, 0, 'P'},
		{"remap", no_argument, 0, 'R'},
		{0, 0, 0, 0}
	};

	Args args = {
		.biosfile = "roms/bios.bin",
		.debug = 0,
		.link_host_port = 0,
		.link_connect_addr = NULL,
		.volume = -1,
		.volume_set = 0,
		.mute = -1,
		.palette = NULL,
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "hvdb:H:C:V:MP:R", long_options, NULL)) != -1) {
		switch (opt) {
		case 'd': {
			printf("DEBUG MODE active\n");
			args.debug = 1;
			break;
		}
		case 'b': {
			printf("BIOS: %s\n", optarg);
			args.biosfile = optarg;
			break;
		}
		case 'C': args.link_connect_addr = optarg; break;
		case 'H': {
			uint16_t port;
			if (!parse_port(optarg, &port)) {
				fprintf(stderr, "Invalid port: %s\n", optarg);
				exit(1);
			}
			args.link_host_port = port;
			break;
		}
		case 'V': {
			char *end;
			errno = 0;
			long v = strtol(optarg, &end, 10);
			if (errno || *end != '\0' || v < 0 || v > 100) {
				fprintf(stderr, "Invalid volume: %s (expected 0..100)\n", optarg);
				exit(1);
			}
			args.volume = (int)v;
			args.volume_set = 1;
			break;
		}
		case 'M': args.mute = 1; break;
		case 'P': args.palette = optarg; break;

		case 'R': run_remap(); exit(0);
		case 'h': print_usage(argv[0]); exit(0);
		case 'v': print_version(); exit(0);
		default: print_err(argv[0]); exit(1);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No ROM found\n");
		print_err(argv[0]);
		exit(1);
	}
	args.romfile = argv[optind];
	return args;
}

static void init_link (GB *gb, Args args)
{
	if (args.link_host_port > 0 && args.link_connect_addr)
		fprintf(stderr, "Warning: --link-host and --link-connect are mutually exclusive; using --link-host\n");

	if (args.link_host_port > 0) {
		if (link_host(&gb->link, (uint16_t)args.link_host_port))
			gb->serial.link = &gb->link;

	} else if (args.link_connect_addr) {
		char ip[64] = {0};
		char *colon = strchr(args.link_connect_addr, ':');
		if (colon) {
			size_t len = (size_t)(colon - args.link_connect_addr);
			if (len >= sizeof(ip)) len = sizeof(ip) - 1;
			memcpy(ip, args.link_connect_addr, len);

			uint16_t port;
			if (!parse_port(colon + 1, &port)) {
				fprintf(stderr, "Invalid port in --link-connect: %s\n", colon + 1);
				return;
			}
			if (link_connect(&gb->link, ip, port))
				gb->serial.link = &gb->link;
		} else {
			fprintf(stderr, "Expected format: --link-connect IP:PORT\n");
		}
	}
}

static void init_config (GB *gb, Args args)
{
	load_config(&gb->cfg);
	gb->ppu.palette = gb->cfg.palette;

	if (args.volume_set)
		atomic_store(&gb->cfg.volume, args.volume);

	if (args.mute >= 0)
		atomic_store(&gb->cfg.muted, (uint8_t)args.mute);

	if (!args.palette) return;
	for (int i = 0; i < PAL_COUNT; i++) {
		const char *n = palette_name((DmgPalette)i);
		if (!strcmp(args.palette, n)) {
			gb->cfg.palette = (DmgPalette)i;
			gb->ppu.palette = (DmgPalette)i;
			break;
		}
	}
}

static int init_emulator (GB *gb, const char *romfile, const char *biosfile) {
	init(gb, romfile, biosfile);
	if (!gb->running) {
		cleanup(gb, romfile);
		return 0;
	}
	return 1;
}

static int run_frame (GB *gb, uint32_t max_queued)
{
	while (1) {
		gb_step(gb);

		if (gb->cfg.turbo) {
			gb->apu.buffer_pos = 0;

		} else if (gb->apu.buffer_pos >= 256) {
			queue_audio(gb);
			while (ring_used(&gb->audio.ring) > max_queued << 1)
				SDL_Delay(1);
		}

		if ((gb->clock & 511) == 0)
			if (!handle_events(gb)) return 0;

		if (gb->ppu.ready) return 1;
	}
	return 1;
}

static void sleep_until (uint64_t target, uint64_t freq, Link *link, uint32_t rx_mark)
{
	while (1) {
		uint64_t now = SDL_GetPerformanceCounter();
		if (now >= target) break;
		if (link && link_is_connected(link) && link_rx_activity(link) != rx_mark)
			break;
		uint64_t remaining_ms = (target - now) * 1000 / freq;
		if (remaining_ms > 2) SDL_Delay(1);
	}
}

static void sync_frame (GB *gb, uint64_t *next_frame, uint64_t frame_ticks, uint64_t freq)
{
	if (gb->cfg.turbo) frame_ticks /= 3;

	*next_frame += frame_ticks;
	uint64_t now = SDL_GetPerformanceCounter();
	if (*next_frame < now) *next_frame = now;

	Link *link = gb->serial.link;
	uint32_t rx_mark = link ? link_rx_activity(link) : 0;
	sleep_until(*next_frame, freq, link, rx_mark);
}

static void run (GB *gb, const char *romfile)
{
	double frames_per_sec = ((double)GB_CLOCK_HZ) / ((double)TICKS_PER_FRAME);
	uint32_t samples_per_frame = (uint32_t)((double)gb->audio.sample_rate / frames_per_sec * 2.0);
	uint32_t max_queued = (uint32_t)(samples_per_frame * 3.0);

	uint64_t freq = SDL_GetPerformanceFrequency();
	uint64_t next_frame = SDL_GetPerformanceCounter();

	uint32_t frames_since_save = 0;

	while (gb->running && !quit_requested) {

		if (!run_frame(gb, max_queued)) {
			gb->running = 0;
			break;
		}

		uint64_t frame_ticks = gb->clock * freq / GB_CLOCK_HZ;
		gb->clock = 0;

		queue_audio(gb);
		update_screen(&gb->lcd, gb->ppu.framebuffer, gb->memory.cart.rumble_on);
		update_rumble(&gb->pad, gb->memory.cart.rumble_on);

		if (gb->memory.cart.save_needed && ++frames_since_save >= AUTOSAVE_RATE_FRAMES) {
			gb->memory.cart.save_needed = 0;
			frames_since_save = 0;
			request_save(gb);
		}

		if (!gb->cfg.turbo) {
			while (ring_used(&gb->audio.ring) > max_queued) SDL_Delay(1);
		}

		sync_frame(gb, &next_frame, frame_ticks, freq);
	}

	cleanup(gb, romfile);
}

int main (int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	Args args = parse_args(argc, argv);

	if (args.debug)
		return run_test(args.romfile);


	GB *gb = malloc(sizeof(GB));
	if (!gb) {
		fprintf(stderr, "FAIL: Not enough memory. Buy more RAM.\n");
		return 1;
	}

	if (!init_emulator(gb, args.romfile, args.biosfile)) {
		free(gb);
		return 1;
	}
	init_config(gb, args);
	init_link(gb, args);
	SDL_PauseAudioDevice(gb->audio.dev, 0);
	start_save_thread(gb, args.romfile);

	run(gb, args.romfile);

	save_config(&gb->cfg);
	free(gb);
	return 0;
}
