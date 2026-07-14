#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <SDL2/SDL.h>

typedef struct Config Config;

typedef enum {
	PAL_DEFAULT,
	PAL_POCKET,
	PAL_BGB,
	PAL_CHOCO,
	PAL_POCKET_GREEN,
	PAL_COUNT
} DmgPalette;

extern const uint32_t PALETTES[PAL_COUNT][5];

typedef struct {
	SDL_Scancode right, left, up, down;
	SDL_Scancode a, b, start, select;
	SDL_Scancode turbo, mute, palette;
	SDL_Scancode vol_up, vol_down;
} Keymap;

void default_keymap (Keymap *k);
const char *palette_name (DmgPalette p);

void load_config (Config *cfg);
void save_config (Config *cfg);

void run_remap (void);

#endif
