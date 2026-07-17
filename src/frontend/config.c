#include "frontend/config.h"
#include "gb.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <SDL2/SDL_ttf.h>

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
	},
	[PAL_BASIC] = {
		0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000, 0xFFFFFFFF
	}
};

const ActionMeta ACTIONS[ACT_COUNT] =
{
	[ACT_RIGHT]	= { "RIGHT",	"right",	offsetof(Keymap, right),	offsetof(Padmap, right)	},
	[ACT_LEFT]	= { "LEFT",	"left",		offsetof(Keymap, left),		offsetof(Padmap, left)	},
	[ACT_UP]	= { "UP",	"up",		offsetof(Keymap, up),		offsetof(Padmap, up)	},
	[ACT_DOWN]	= { "DOWN",	"down",		offsetof(Keymap, down),		offsetof(Padmap, down)	},
	[ACT_A]		= { "A",	"a",		offsetof(Keymap, a),		offsetof(Padmap, a)	},
	[ACT_B]		= { "B",	"b",		offsetof(Keymap, b),		offsetof(Padmap, b)	},
	[ACT_START]	= { "START",	"start",	offsetof(Keymap, start),	offsetof(Padmap, start)	},
	[ACT_SELECT]	= { "SELECT",	"select",	offsetof(Keymap, select),	offsetof(Padmap, select)},
	[ACT_TURBO]	= { "TURBO",	"turbo",	offsetof(Keymap, turbo),	offsetof(Padmap, turbo)	},
	[ACT_MUTE]	= { "MUTE",	"mute",		offsetof(Keymap, mute),		offsetof(Padmap, mute)	},
	[ACT_PALETTE]	= { "PALETTE",	"palette",	offsetof(Keymap, palette),	offsetof(Padmap, palette)},
	[ACT_VOL_UP]	= { "VOLUME UP","vol_up",	offsetof(Keymap, vol_up),	offsetof(Padmap, vol_up)},
	[ACT_VOL_DOWN]	= { "VOLUME DOWN","vol_down",	offsetof(Keymap, vol_down),	offsetof(Padmap, vol_down)}
};

Keybind kb_binding (const Keymap *k, Action a)
{
	return *(Keybind *)((const char *)k + ACTIONS[a].kb_offset);
}

SDL_GameControllerButton pad_binding (const Padmap *p, Action a)
{
	return *(SDL_GameControllerButton *)((const char *)p + ACTIONS[a].pad_offset);
}

void set_kb_binding (Keymap *k, Action a, Keybind kb)
{
	*(Keybind *)((char *)k + ACTIONS[a].kb_offset) = kb;
}

void set_pad_binding (Padmap *p, Action a, SDL_GameControllerButton b)
{
	*(SDL_GameControllerButton *)((char *)p + ACTIONS[a].pad_offset) = b;
}

void init_config_defaults (Config *cfg)
{
	memset(cfg, 0, sizeof(Config));
	default_keymap(&cfg->keymap);
	default_padmap(&cfg->padmap);
	atomic_store(&cfg->volume, 100);
	atomic_store(&cfg->muted, 0);
	cfg->palette = PAL_DEFAULT;
}

static struct {
	const char *name;
	SDL_Scancode code;
} const SCANCODE_TABLE[] = {
	{"RIGHT",	SDL_SCANCODE_RIGHT},
	{"LEFT",	SDL_SCANCODE_LEFT},
	{"UP",		SDL_SCANCODE_UP},
	{"DOWN",	SDL_SCANCODE_DOWN},
	{"A",		SDL_SCANCODE_A},
	{"B",		SDL_SCANCODE_B},
	{"C",		SDL_SCANCODE_C},
	{"D",		SDL_SCANCODE_D},
	{"E",		SDL_SCANCODE_E},
	{"F",		SDL_SCANCODE_F},
	{"G",		SDL_SCANCODE_G},
	{"H",		SDL_SCANCODE_H},
	{"I",		SDL_SCANCODE_I},
	{"J",		SDL_SCANCODE_J},
	{"K",		SDL_SCANCODE_K},
	{"L",		SDL_SCANCODE_L},
	{"M",		SDL_SCANCODE_M},
	{"N",		SDL_SCANCODE_N},
	{"O",		SDL_SCANCODE_O},
	{"P",		SDL_SCANCODE_P},
	{"Q",		SDL_SCANCODE_Q},
	{"R",		SDL_SCANCODE_R},
	{"S",		SDL_SCANCODE_S},
	{"T",		SDL_SCANCODE_T},
	{"U",		SDL_SCANCODE_U},
	{"V",		SDL_SCANCODE_V},
	{"W",		SDL_SCANCODE_W},
	{"X",		SDL_SCANCODE_X},
	{"Y",		SDL_SCANCODE_Y},
	{"Z",		SDL_SCANCODE_Z},
	{"0",		SDL_SCANCODE_0},
	{"1",		SDL_SCANCODE_1},
	{"2",		SDL_SCANCODE_2},
	{"3",		SDL_SCANCODE_3},
	{"4",		SDL_SCANCODE_4},
	{"5",		SDL_SCANCODE_5},
	{"6",		SDL_SCANCODE_6},
	{"7",		SDL_SCANCODE_7},
	{"8",		SDL_SCANCODE_8},
	{"9",		SDL_SCANCODE_9},
	{"RETURN",	SDL_SCANCODE_RETURN},
	{"ENTER",	SDL_SCANCODE_RETURN},
	{"SPACE",	SDL_SCANCODE_SPACE},
	{"BACKSPACE",	SDL_SCANCODE_BACKSPACE},
	{"TAB",		SDL_SCANCODE_TAB},
	{"ESCAPE",	SDL_SCANCODE_ESCAPE},
	{"ESC",		SDL_SCANCODE_ESCAPE},
	{"MINUS",	SDL_SCANCODE_MINUS},
	{"PLUS",	SDL_SCANCODE_KP_PLUS},
	{"EQUALS",	SDL_SCANCODE_EQUALS},
	{"LCTRL",	SDL_SCANCODE_LCTRL},
	{"RCTRL",	SDL_SCANCODE_RCTRL},
	{"LSHIFT",	SDL_SCANCODE_LSHIFT},
	{"RSHIFT",	SDL_SCANCODE_RSHIFT},
	{"LALT",	SDL_SCANCODE_LALT},
	{"RALT",	SDL_SCANCODE_RALT},
	{"KP_PLUS",	SDL_SCANCODE_KP_PLUS},
	{"KP_MINUS",	SDL_SCANCODE_KP_MINUS},
	{"KP_EQUALS",	SDL_SCANCODE_KP_EQUALS},
	{"KP_0",	SDL_SCANCODE_KP_0},
	{"KP_1",	SDL_SCANCODE_KP_1},
	{"KP_2",	SDL_SCANCODE_KP_2},
	{"KP_3",	SDL_SCANCODE_KP_3},
	{"KP_4",	SDL_SCANCODE_KP_4},
	{"KP_5",	SDL_SCANCODE_KP_5},
	{"KP_6",	SDL_SCANCODE_KP_6},
	{"KP_7",	SDL_SCANCODE_KP_7},
	{"KP_8",	SDL_SCANCODE_KP_8},
	{"KP_9",	SDL_SCANCODE_KP_9},
	{NULL,		0}
};

static int scancode_from_name (const char *name)
{
	if (!name || !*name) return -1;
	char buf[32];

	int n = sizeof(buf) - 1;
	int i;
	for (i = 0; i < n && name[i]; i++) {
		buf[i] = (char)toupper((unsigned char)name[i]);
	}
	buf[i] = 0;

	for (int j = 0; SCANCODE_TABLE[j].name; j++) {
		if (strcmp(buf, SCANCODE_TABLE[j].name) == 0)
			return (int)SCANCODE_TABLE[j].code;
	}

	return -1;
}

static const char *scancode_to_name (SDL_Scancode s)
{
	for (int j = 0; SCANCODE_TABLE[j].name; j++) {
		if (SCANCODE_TABLE[j].code == s)
			return SCANCODE_TABLE[j].name;
	}
	return NULL;
}

int parse_keybind_token (const char *token, Keybind *out)
{
	out->scancode = SDL_SCANCODE_UNKNOWN;
	out->mods = 0;
	if (!token || !*token) return -1;

	uint16_t mods = 0;
	const char *p = token;

	while (1) {
		const char *plus = strchr(p, '+');
		size_t comp_len = plus ? (size_t)(plus - p) : strlen(p);
		char comp[32];
		if (comp_len >= sizeof(comp)) return -1;
		memcpy(comp, p, comp_len);
		comp[comp_len] = 0;

		char cu[32];
		for (size_t i = 0; i < sizeof(cu) - 1 && comp[i]; i++) {
			cu[i] = (char)toupper((unsigned char)comp[i]);
		}
		cu[comp_len] = 0;

		int matched_mod = 1;
		if	(!strcmp(cu, "CTRL") || !strcmp(cu, "CONTROL")) mods |= KBMOD_CTRL;
		else if (!strcmp(cu, "SHIFT")) mods |= KBMOD_SHIFT;
		else if (!strcmp(cu, "ALT") || !strcmp(cu, "OPTION")) mods |= KBMOD_ALT;
		else if (!strcmp(cu, "GUI") || !strcmp(cu, "CMD")
			|| !strcmp(cu, "SUPER") || !strcmp(cu, "META")) mods |= KBMOD_GUI;
		else matched_mod = 0;

		if (matched_mod) {
			if (!plus) return -1;
			p = plus + 1;
			continue;
		}

		int sc = scancode_from_name(comp);
		if (sc < 0) return -1;
		out->scancode = (SDL_Scancode)sc;
		out->mods = mods;
		if (plus) return -1;
		return 0;
	}
}

void write_keybind_token (Keybind kb, char *buf, size_t n)
{
	if (n == 0) return;
	buf[0] = 0;
	const char *sc = scancode_to_name(kb.scancode);
	if (!sc) sc = "UNKNOWN";

	char tmp[64];
	tmp[0] = 0;
	if (kb.mods & KBMOD_CTRL) strncat(tmp, "Ctrl+", sizeof(tmp) - strlen(tmp) - 1);
	if (kb.mods & KBMOD_SHIFT) strncat(tmp, "Shift+", sizeof(tmp) - strlen(tmp) - 1);
	if (kb.mods & KBMOD_ALT) strncat(tmp, "Alt+", sizeof(tmp) - strlen(tmp) - 1);
	if (kb.mods & KBMOD_GUI) strncat(tmp, "GUI+", sizeof(tmp) - strlen(tmp) - 1);
	strncat(tmp, sc, sizeof(tmp) - strlen(tmp) - 1);
	snprintf(buf, n, "%s", tmp);
}

int parse_pad_button_name (const char *name, SDL_GameControllerButton *out)
{
	if (!name || !*name) return -1;
	SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(name);
	if (b == SDL_CONTROLLER_BUTTON_INVALID) {
		char cu[16];
		int n = sizeof(cu) - 1;
		int i;
		for (i = 0; i < n && name[i]; i++) {
			cu[i] = (char)toupper((unsigned char)name[i]);
		}
		cu[i] = 0;

		if (!strcmp(cu, "NONE") || !strcmp(cu, "-") || !strcmp(cu, "—")) {
			*out = SDL_CONTROLLER_BUTTON_INVALID;
			return 0;
		}
		return -1;
	}
	*out = b;
	return 0;
}

const char *pad_button_name (SDL_GameControllerButton b)
{
	if (b == SDL_CONTROLLER_BUTTON_INVALID) return "NONE";
	const char *n = SDL_GameControllerGetStringForButton(b);
	return n ? n : "NONE";
}

void default_keymap (Keymap *k)
{
	static const struct { Action a; SDL_Scancode sc; } defs[] = {
		{ ACT_RIGHT,	SDL_SCANCODE_RIGHT	},
		{ ACT_LEFT,	SDL_SCANCODE_LEFT	},
		{ ACT_UP,	SDL_SCANCODE_UP		},
		{ ACT_DOWN,	SDL_SCANCODE_DOWN	},
		{ ACT_A,	SDL_SCANCODE_X		},
		{ ACT_B,	SDL_SCANCODE_Z		},
		{ ACT_START,	SDL_SCANCODE_RETURN	},
		{ ACT_SELECT,	SDL_SCANCODE_BACKSPACE	},
		{ ACT_TURBO,	SDL_SCANCODE_TAB	},
		{ ACT_MUTE,	SDL_SCANCODE_M		},
		{ ACT_PALETTE,	SDL_SCANCODE_P		},
		{ ACT_VOL_UP,	SDL_SCANCODE_EQUALS	},
		{ ACT_VOL_DOWN,	SDL_SCANCODE_MINUS	},
	};
	int n = sizeof(defs) / sizeof(defs[0]);
	for (int i = 0; i < n; i++) {
		Keybind kb = {
			.scancode = defs[i].sc,
			.mods = 0,
		};
		set_kb_binding(k, defs[i].a, kb);
	}
}

void default_padmap (Padmap *p)
{
	static const struct {
		Action a;
		SDL_GameControllerButton b;
	} defs[] = {
		{ ACT_RIGHT,	SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
		{ ACT_LEFT,	SDL_CONTROLLER_BUTTON_DPAD_LEFT },
		{ ACT_UP,	SDL_CONTROLLER_BUTTON_DPAD_UP	},
		{ ACT_DOWN,	SDL_CONTROLLER_BUTTON_DPAD_DOWN },
		{ ACT_A,	SDL_CONTROLLER_BUTTON_A		},
		{ ACT_B,	SDL_CONTROLLER_BUTTON_B		},
		{ ACT_START,	SDL_CONTROLLER_BUTTON_START	},
		{ ACT_SELECT,	SDL_CONTROLLER_BUTTON_BACK	},
		{ ACT_TURBO,	SDL_CONTROLLER_BUTTON_INVALID	},
		{ ACT_MUTE,	SDL_CONTROLLER_BUTTON_INVALID	},
		{ ACT_PALETTE,	SDL_CONTROLLER_BUTTON_INVALID	},
		{ ACT_VOL_UP,	SDL_CONTROLLER_BUTTON_INVALID	},
		{ ACT_VOL_DOWN,	SDL_CONTROLLER_BUTTON_INVALID	},
	};
	int n = sizeof(defs)/sizeof(defs[0]);
	for (int i = 0; i < n; i++)
		set_pad_binding(p, defs[i].a, defs[i].b);
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
		case PAL_BASIC:		return "basic";
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

static int action_ini_name (const char *name)
{
	if (!name || !*name) return -1;
	for (int i = 0; i < ACT_COUNT; i++)
		if (strcmp(name, ACTIONS[i].ini_name) == 0)
			return i;
	return -1;
}

static int parse_control (const char *line, char *value, int *ai)
{
	char field[32] = {0};
	if (sscanf(line, " %31[^= ] = %63s", field, value) < 2) return 1;

	for (char *p = field; *p; p++)
		*p = (char)tolower((unsigned char)*p);

	*ai = action_ini_name(field);
	if (*ai < 0) return 1;
	return 0;
}

static void parse_keymap_line (Keymap *k, const char *line)
{
	char value[64] = {0};
	int ai;
	if (parse_control(line, value, &ai)) return;

	Keybind kb;
	if (parse_keybind_token(value, &kb) != 0) return;

	set_kb_binding(k, (Action)ai, kb);
}

static void parse_gamepad_line (Padmap *p, const char *line)
{
	char value[64] = {0};
	int ai;
	if (parse_control(line, value, &ai)) return;

	SDL_GameControllerButton b;
	if (parse_pad_button_name(value, &b) != 0) return;
	set_pad_binding(p, (Action)ai, b);
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

		if	(!strcmp(section, "keymap"))  parse_keymap_line  (&cfg->keymap, s);
		else if (!strcmp(section, "gamepad")) parse_gamepad_line (&cfg->padmap, s);
		else if (!strcmp(section, "audio"))  load_audio (cfg, s);
		else if (!strcmp(section, "video"))  load_video (cfg, s);
	}
	fclose(f);
}

static void write_cfg_file (Config *cfg, FILE *f)
{
	fprintf(f, "; r2boy configuration\n\n");

	fprintf(f, "[audio]\n");
	fprintf(f, "volume = %d\n", atomic_load(&cfg->volume));
	fprintf(f, "muted  = %d\n\n", atomic_load(&cfg->muted));

	fprintf(f, "[video]\n");
	fprintf(f, "palette = %s\n\n", palette_name(cfg->palette));

	fprintf(f, "[keymap]\n");
	for (int i = 0; i < ACT_COUNT; i++) {
		char tok[64];
		write_keybind_token(kb_binding(&cfg->keymap, (Action)i), tok, sizeof(tok));
		fprintf(f, "%s = %s\n", ACTIONS[i].ini_name, tok);
	}

	fprintf(f, "\n[gamepad]\n");
	for (int i = 0; i < ACT_COUNT; i++) {
		fprintf(f, "%s = %s\n", ACTIONS[i].ini_name,
			pad_button_name(pad_binding(&cfg->padmap, (Action)i)));
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
