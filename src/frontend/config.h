#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdatomic.h>
#include <SDL2/SDL.h>
#include "frontend/remap_ui.h"

typedef struct Config Config;

typedef enum {
	PAL_DEFAULT,
	PAL_POCKET,
	PAL_BGB,
	PAL_CHOCO,
	PAL_POCKET_GREEN,
	PAL_BASIC,
	PAL_COUNT
} DmgPalette;

extern const uint32_t PALETTES[PAL_COUNT][5];

#define KBMOD_CTRL (KMOD_CTRL)
#define KBMOD_SHIFT (KMOD_SHIFT)
#define KBMOD_ALT (KMOD_ALT)
#define KBMOD_GUI (KMOD_GUI)
#define KBMOD_ANY (KBMOD_CTRL | KBMOD_SHIFT | KBMOD_ALT | KBMOD_GUI)

typedef struct {
	SDL_Scancode scancode;
	uint16_t mods;
} Keybind;

typedef struct {
	Keybind right, left, up, down;
	Keybind a, b, start, select;
	Keybind turbo, mute, palette;
	Keybind vol_up, vol_down;
	Keybind save_1, load_1, save_2, load_2;
	Keybind tilt_right, tilt_left, tilt_up, tilt_down;
} Keymap;

typedef struct {
	SDL_GameControllerButton right, left, up, down;
	SDL_GameControllerButton a, b, start, select;
	SDL_GameControllerButton turbo, mute, palette, vol_up, vol_down;
	SDL_GameControllerButton save_1, load_1, save_2, load_2;
	SDL_GameControllerButton tilt_right, tilt_left, tilt_up, tilt_down;
} Padmap;

typedef enum {
	ACT_RIGHT = 0, ACT_LEFT, ACT_UP, ACT_DOWN,
	ACT_A, ACT_B, ACT_START, ACT_SELECT,
	ACT_TURBO, ACT_SAVE1, ACT_LOAD1, ACT_SAVE2, ACT_LOAD2,
	ACT_MUTE, ACT_PALETTE, ACT_VOL_UP, ACT_VOL_DOWN,
	ACT_TILT_RIGHT, ACT_TILT_LEFT, ACT_TILT_UP, ACT_TILT_DOWN,
	ACT_COUNT
} Action;

typedef struct {
	const char *label;
	const char *ini_name;
	size_t kb_offset;
	size_t pad_offset;
} ActionMeta;

extern const ActionMeta ACTIONS[ACT_COUNT];

typedef struct Config {
	Keymap keymap;
	Padmap padmap;
	uint8_t turbo;
	_Atomic int volume;
	_Atomic uint8_t muted;
	DmgPalette palette;
	uint8_t use_acel;
} Config;

void default_keymap (Keymap *k);
void default_padmap (Padmap *p);
const char *palette_name (DmgPalette p);
void init_config_defaults(Config *cfg);

Keybind kb_binding (const Keymap *k, Action a);
SDL_GameControllerButton pad_binding (const Padmap *p, Action a);
void set_kb_binding (Keymap *k, Action a, Keybind kb);
void set_pad_binding (Padmap *p, Action a, SDL_GameControllerButton b);

void write_keybind_token (Keybind kb, char *buf, size_t n);
const char *pad_button_name (SDL_GameControllerButton b);

void load_config (Config *cfg);
void save_config (Config *cfg);

#endif
