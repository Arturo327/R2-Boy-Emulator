#include "frontend/config.h"
#include "gb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

const uint32_t PALETTES[PAL_COUNT][5] = {
	[PAL_DEFAULT] = {
		0xFFC6DE8C, 0xFF84A563, 0xFF396139, 0xFF081810, 0xFFD2E6A6
	},
	[PAL_POCKET] = {
		0xFFE0DBCD, 0xFFA89F94, 0xFF706B66, 0xFF2B2B26, 0xFFEEE9DB
	},
	[PAL_BGB] = {
		0xFFE0F8D0, 0xFF88C070, 0xFF346856, 0xFF081820, 0xFFEEFFDE
	},
	[PAL_CHOCO] = {
		0xFFFFE4C2, 0xFFDCA456, 0xFFA9604C, 0xFF422936, 0xFFFFFFE0
	},
	[PAL_POCKET_GREEN] = {
		0xFFDBF4B4, 0xFFABC396, 0xFF7B9278, 0xFF4C625A, 0xFFEAFFC0
	}
};

void default_keymap (Keymap *k) {
	k->right = SDL_SCANCODE_RIGHT;
	k->left = SDL_SCANCODE_LEFT;
	k->up = SDL_SCANCODE_UP;
	k->down = SDL_SCANCODE_DOWN;
	k->a = SDL_SCANCODE_X;
	k->b = SDL_SCANCODE_Z;
	k->start = SDL_SCANCODE_RETURN;
	k->select = SDL_SCANCODE_BACKSPACE;
	k->turbo = SDL_SCANCODE_TAB;
	k->mute = SDL_SCANCODE_M;
	k->palette = SDL_SCANCODE_P;
	k->vol_up = SDL_SCANCODE_0;
	k->vol_down = SDL_SCANCODE_9;
}

static struct {
	char *name;
	SDL_Scancode code;
} SCANCODE_TABLE[] =
{
	{"RIGHT", SDL_SCANCODE_RIGHT},
	{"LEFT", SDL_SCANCODE_LEFT},
	{"UP", SDL_SCANCODE_UP},
	{"DOWN", SDL_SCANCODE_DOWN},
	{"A", SDL_SCANCODE_A},
	{"B", SDL_SCANCODE_B},
	{"C", SDL_SCANCODE_C},
	{"D", SDL_SCANCODE_D},
	{"E", SDL_SCANCODE_E},
	{"F", SDL_SCANCODE_F},
	{"G", SDL_SCANCODE_G},
	{"H", SDL_SCANCODE_H},
	{"I", SDL_SCANCODE_I},
	{"J", SDL_SCANCODE_J},
	{"K", SDL_SCANCODE_K},
	{"L", SDL_SCANCODE_L},
	{"M", SDL_SCANCODE_M},
	{"N", SDL_SCANCODE_N},
	{"O", SDL_SCANCODE_O},
	{"P", SDL_SCANCODE_P},
	{"Q", SDL_SCANCODE_Q},
	{"R", SDL_SCANCODE_R},
	{"S", SDL_SCANCODE_S},
	{"T", SDL_SCANCODE_T},
	{"U", SDL_SCANCODE_U},
	{"V", SDL_SCANCODE_V},
	{"W", SDL_SCANCODE_W},
	{"X", SDL_SCANCODE_X},
	{"Y", SDL_SCANCODE_Y},
	{"Z", SDL_SCANCODE_Z},
	{"0", SDL_SCANCODE_0},
	{"1", SDL_SCANCODE_1},
	{"2", SDL_SCANCODE_2},
	{"3", SDL_SCANCODE_3},
	{"4", SDL_SCANCODE_4},
	{"5", SDL_SCANCODE_5},
	{"6", SDL_SCANCODE_6},
	{"7", SDL_SCANCODE_7},
	{"8", SDL_SCANCODE_8},
	{"9", SDL_SCANCODE_9},
	{"RETURN", SDL_SCANCODE_RETURN},
	{"ENTER", SDL_SCANCODE_RETURN},
	{"SPACE", SDL_SCANCODE_SPACE},
	{"BACKSPACE", SDL_SCANCODE_BACKSPACE},
	{"TAB", SDL_SCANCODE_TAB},
	{"ESCAPE", SDL_SCANCODE_ESCAPE},
	{"ESC", SDL_SCANCODE_ESCAPE},
	{"MINUS", SDL_SCANCODE_MINUS},
	{"PLUS", SDL_SCANCODE_KP_PLUS},
	{"EQUALS", SDL_SCANCODE_EQUALS},
	{"LCTRL", SDL_SCANCODE_LCTRL},
	{"RCTRL", SDL_SCANCODE_RCTRL},
	{"LSHIFT", SDL_SCANCODE_LSHIFT},
	{"RSHIFT", SDL_SCANCODE_RSHIFT},
	{"LALT", SDL_SCANCODE_LALT},
	{"RALT", SDL_SCANCODE_RALT},
	{NULL, 0}
};

static int scancode_from_name (const char *name)
{
	if (!name || !*name) return -1;
	char buf[32];
	size_t i;
	for (i = 0; i < sizeof(buf) - 1 && name[i]; i++) {
		buf[i] = (char)toupper((unsigned char)name[i]);
	}
	buf[i] = 0;

	for (int j = 0; SCANCODE_TABLE[j].name; j++) {
		if (strcmp(buf, SCANCODE_TABLE[j].name) == 0)
			return (int)SCANCODE_TABLE[j].code;
	}
	return -1;
}

static char *scancode_to_name (SDL_Scancode s)
{
	for (int j = 0; SCANCODE_TABLE[j].name; j++)
		if (SCANCODE_TABLE[j].code == s)
			return SCANCODE_TABLE[j].name;
	return NULL;
}

static int set_keymap_field (Keymap *k, const char *field, SDL_Scancode code)
{
	if	(!strcmp(field, "right"))	k->right	= code;
	else if (!strcmp(field, "left"))	k->left		= code;
	else if (!strcmp(field, "up"))		k->up		= code;
	else if (!strcmp(field, "down"))	k->down		= code;
	else if (!strcmp(field, "a"))		k->a		= code;
	else if (!strcmp(field, "b"))		k->b		= code;
	else if (!strcmp(field, "start"))	k->start	= code;
	else if (!strcmp(field, "select"))	k->select	= code;
	else if (!strcmp(field, "turbo"))	k->turbo	= code;
	else if (!strcmp(field, "mute"))	k->mute		= code;
	else if (!strcmp(field, "palette"))	k->palette	= code;
	else if (!strcmp(field, "vol_up"))	k->vol_up	= code;
	else if (!strcmp(field, "vol_down"))	k->vol_down	= code;
	else return 0;
	return 1;
}

static int parse_keymap_line (Keymap *k, const char *line)
{
	char field[32] = {0};
	char value[32] = {0};
	if (sscanf(line, " %31[^= ] = %31s", field, value) < 2) return 0;

	for (char *p = field; *p; p++) {
		*p = (char)tolower((unsigned char)*p);
	}

	int code = scancode_from_name(value);
	if (code < 0) return 0;
	return set_keymap_field(k, field, (SDL_Scancode)code);
}

static void config_path (char *out, size_t outsize)
{
	const char *home = getenv("XDG_CONFIG_HOME");
	if (home && *home) {
		snprintf(out, outsize, "%s/r2boy", home);
	} else {
		home = getenv("HOME");
		if (!home) home = ".";
		snprintf(out, outsize, "%s/.config/r2boy", home);
	}
}

static void mkdir_p (const char *path)
{
	struct stat st;
	if (stat(path, &st) == 0) return;
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "%s", path);
	size_t len = strlen(tmp);

	if (len > 0 && tmp[len-1] == '/')
		tmp[len-1] = 0;

	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
}

static void load_audio (Config *cfg, const char *s)
{
	char field[32];
	char value[32];
	if (sscanf(s, " %31[^= ] = %31s", field, value) != 2) return;

	if (!strcmp(field, "volume")) {
		int v = atoi(value);
		if (v < 0) v = 0;
		if (v > 100) v = 100;
		atomic_store(&cfg->volume, v);

	} else if (!strcmp(field, "muted")) {
		uint8_t v = (uint8_t)(value[0]=='1' || value[0]=='t' || value[0]=='T');
		atomic_store(&cfg->muted, v);
	}
}

const char *palette_name (DmgPalette p) {
	switch (p) {
		case PAL_DEFAULT:	return "DMG";
		case PAL_POCKET:	return "pocket";
		case PAL_BGB:		return "BGB";
		case PAL_CHOCO:		return "choco";
		case PAL_POCKET_GREEN:	return "pocket_green";
		default:		return "unknown";
	}
}

static void load_video (Config *cfg, const char *s)
{
	char field[32];
	char value[32];
	if (sscanf(s, " %31[^= ] = %31s", field, value) != 2) return;

	if (strcmp(field, "palette")) return;
	for (int i = 0; i < PAL_COUNT; i++) {
		const char *n = palette_name((DmgPalette)i);
		if (!strcmp(value, n)) {
			cfg->palette = (DmgPalette)i;
			break;
		}
	}
}

void load_config (Config *cfg)
{
	char dir[512];
	config_path(dir, sizeof(dir));
	mkdir_p(dir);

	char path[600];
	snprintf(path, sizeof(path), "%s/config.ini", dir);

	FILE *f = fopen(path, "r");
	if (!f) return;

	char line[256];
	char section[32] = {0};
	while (fgets(line, sizeof(line), f)) {

		char *s = line;
		while (*s && isspace((unsigned char)*s)) s++;

		if (*s == '#' || *s == ';' || *s == 0) continue;
		if (*s == '[') {
			char *e = strchr(s, ']');
			if (e) {
				size_t n = (size_t)(e - s - 1);
				if (n >= sizeof(section)) n = sizeof(section) - 1;
				memcpy(section, s + 1, n);
				section[n] = 0;
			}
			continue;
		}

		if (strcmp(section, "keymap") == 0) {
			parse_keymap_line(&cfg->keymap, s);

		} else if (strcmp(section, "audio") == 0) {
			load_audio(cfg, s);

		} else if (strcmp(section, "video") == 0) {
			load_video(cfg, s);
		}
	}
	fclose(f);
}

static void write_cfg_file (Config *cfg, FILE *f)
{
	fprintf(f, "; r2boy configuration\n\n");

	fprintf(f, "[audio]\n");
	fprintf(f, "volume = %d\n", atomic_load(&cfg->volume));
	fprintf(f, "muted = %d\n\n", atomic_load(&cfg->muted));

	fprintf(f, "[video]\n");
	fprintf(f, "palette = %s\n\n", palette_name(cfg->palette));

	fprintf(f, "[keymap]\n");
	const struct { const char *name; SDL_Scancode code; } fields[] =
	{
		{"right",	cfg->keymap.right},
		{"left",	cfg->keymap.left},
		{"up",		cfg->keymap.up},
		{"down",	cfg->keymap.down},
		{"a",		cfg->keymap.a},
		{"b",		cfg->keymap.b},
		{"start",	cfg->keymap.start},
		{"select",	cfg->keymap.select},
		{"turbo",	cfg->keymap.turbo},
		{"mute",	cfg->keymap.mute},
		{"palette",	cfg->keymap.palette},
		{"vol_up",	cfg->keymap.vol_up},
		{"vol_down",	cfg->keymap.vol_down},
		{NULL, 0}
	};
	for (int i = 0; fields[i].name; i++) {
		const char *n = scancode_to_name(fields[i].code);
		fprintf(f, "%s = %s\n", fields[i].name, n ? n : "UNKNOWN");
	}
}

void save_config (Config *cfg)
{
	char dir[512];
	config_path(dir, sizeof(dir));
	mkdir_p(dir);

	char path[600];
	snprintf(path, sizeof(path), "%s/config.ini", dir);

	FILE *f = fopen(path, "w");
	if (!f) return;

	write_cfg_file(cfg, f);
	fclose(f);
}

#define KEY_NUM 13

static struct { char *label; size_t offset; } remap_keys[KEY_NUM] =
{
	{"RIGHT",	offsetof(Keymap, right)},
	{"LEFT",	offsetof(Keymap, left)},
	{"UP",		offsetof(Keymap, up)},
	{"DOWN",	offsetof(Keymap, down)},

	{"A",		offsetof(Keymap, a)},
	{"B",		offsetof(Keymap, b)},
	{"START",	offsetof(Keymap, start)},
	{"SELECT",	offsetof(Keymap, select)},

	{"TURBO",	offsetof(Keymap, turbo)},
	{"MUTE",	offsetof(Keymap, mute)},
	{"PALETTE",	offsetof(Keymap, palette)},
	{"VOLUME UP",	offsetof(Keymap, vol_up)},
	{"VOLUME DOWN",	offsetof(Keymap, vol_down)}
};

static SDL_Scancode read_terminal_scancode (void)
{
	unsigned char c;
	if (read(STDIN_FILENO, &c, 1) != 1)
		return 0;

	if (c == 0x1b) {
		unsigned char seq[2];
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 50000;

		if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
			return SDL_SCANCODE_ESCAPE;
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return SDL_SCANCODE_ESCAPE;

		if (seq[0] == '[') {
			FD_ZERO(&fds);
			FD_SET(STDIN_FILENO, &fds);
			tv.tv_sec = 0;
			tv.tv_usec = 50000;
			if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
				return SDL_SCANCODE_ESCAPE;
			if (read(STDIN_FILENO, &seq[1], 1) != 1)
				return SDL_SCANCODE_ESCAPE;

			switch (seq[1])
			{
			case 'A': return SDL_SCANCODE_UP;
			case 'B': return SDL_SCANCODE_DOWN;
			case 'C': return SDL_SCANCODE_RIGHT;
			case 'D': return SDL_SCANCODE_LEFT;
			}
		}
		return SDL_SCANCODE_ESCAPE;
	}

	if (c == '\r' || c == '\n')	return SDL_SCANCODE_RETURN;
	if (c == '\t')			return SDL_SCANCODE_TAB;
	if (c == ' ')			return SDL_SCANCODE_SPACE;
	if (c == 0x7f || c == 0x08)	return SDL_SCANCODE_BACKSPACE;

	if (c == '-')			return SDL_SCANCODE_MINUS;
	if (c == '_')			return SDL_SCANCODE_MINUS;
	if (c == '=')			return SDL_SCANCODE_EQUALS;
	if (c == '+')			return SDL_SCANCODE_KP_PLUS;

	if (c >= '0' && c <= '9')	return (SDL_Scancode)(SDL_SCANCODE_0 + (c - '0'));
	if (c >= 'a' && c <= 'z')	return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'a'));
	if (c >= 'A' && c <= 'Z')	return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'A'));

	if (c == 0x03 || c == 0x1C)
		return SDL_SCANCODE_UNKNOWN;

	return (SDL_Scancode)-1;
}

void run_remap (void)
{
	printf("\n=== REMAP CONFIGURATION ===\n");
	printf("Press a key for each action. ESC cancels, Ctrl+C aborts.\n\n");

	Config *cfg = malloc(sizeof(Config));
	if (!cfg) {
		fprintf(stderr, "Not enough memory. Buy more RAM.\n");
		return;
	}

	memset(cfg, 0, sizeof(Config));
	default_keymap(&cfg->keymap);
	atomic_store(&cfg->volume, 100);
	atomic_store(&cfg->muted, 0);
	cfg->palette = PAL_DEFAULT;
	load_config(cfg);

	struct termios old_tio, new_tio;
	if (tcgetattr(STDIN_FILENO, &old_tio) < 0) {
		fprintf(stderr, "Cannot get terminal attributes\n");
		free(cfg);
		return;
	}

	new_tio = old_tio;
	new_tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP |
				INLCR | IGNCR | ICRNL | IXON);
	new_tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	new_tio.c_cflag &= ~(CSIZE | PARENB);
	new_tio.c_cflag |= CS8;
	new_tio.c_cc[VMIN]  = 1;
	new_tio.c_cc[VTIME] = 0;

	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
	tcflush(STDIN_FILENO, TCIFLUSH);

	for (int i = 0; i < KEY_NUM; i++) {
		printf("%-22s : press key  ", remap_keys[i].label);
		fflush(stdout);

		SDL_Scancode got = read_terminal_scancode();

		if (got == SDL_SCANCODE_UNKNOWN) {
			printf("\nCancelled\n");
			break;
		}

		if (got == SDL_SCANCODE_ESCAPE) {
			const char *curr = scancode_to_name(
				*(SDL_Scancode*)((char*)&cfg->keymap + remap_keys[i].offset));
			printf("(keep %s)\n", curr ? curr : "unknown");
			continue;
		}

		if (got == (SDL_Scancode)-1) {
			i--;
			continue;
		}

		if (got == 0) {
			printf("\nError reading input\n");
			break;
		}

		*(SDL_Scancode*)((char*)&cfg->keymap + remap_keys[i].offset) = got;
		const char *n = scancode_to_name(got);
		printf("%s\n", n ? n : "(unknown)");
	}

	printf("\nKeymap saved to config.\n\n");
	save_config(cfg);

	tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
	free(cfg);
}
