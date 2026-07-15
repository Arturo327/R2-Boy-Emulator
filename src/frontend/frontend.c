#include "frontend/frontend.h"
#include "gb.h"

#include <stdio.h>

#define JOYPAD_RIGHT 0x01
#define JOYPAD_LEFT 0x02
#define JOYPAD_UP 0x04
#define JOYPAD_DOWN 0x08
#define JOYPAD_A 0x10
#define JOYPAD_B 0x20
#define JOYPAD_SELECT 0x40
#define JOYPAD_START  0x80

#define GAMEPAD_DEADZONE 8000

static int handle_window_event (SDL_Event *e)
{
	if (e->type == SDL_QUIT) return 0;
	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_CLOSE)
		return 0;
	return 1;
}

static void handle_hotkey (GB *gb, SDL_Scancode sc, int pressed)
{
	if (!pressed) return;
	Config *cfg = &gb->cfg;

	if (sc == cfg->keymap.mute) {
		uint8_t m = !atomic_load(&cfg->muted);
		atomic_store(&cfg->muted, m);
		fprintf(stderr, "Audio: %s\n", m ? "muted" : "unmuted");

	} else if (sc == cfg->keymap.vol_up) {
		int v = atomic_load(&cfg->volume) + 10;
		if (v > 100) v = 100;
		atomic_store(&cfg->volume, v);
		fprintf(stderr, "Volume: %d%%\n", v);

	} else if (sc == cfg->keymap.vol_down) {
		int v = atomic_load(&cfg->volume) - 10;
		if (v < 0) v = 0;
		atomic_store(&cfg->volume, v);
		fprintf(stderr, "Volume: %d%%\n", v);

	} else if (sc == cfg->keymap.palette) {
		DmgPalette pal = (DmgPalette)((cfg->palette + 1) % PAL_COUNT);
		cfg->palette = pal;
		gb->ppu.palette = pal;
		fprintf(stderr, "Palette: %s\n", palette_name(cfg->palette));
	}
}

static uint8_t handle_kb_event (GB *gb, const SDL_Event *e, uint8_t curr)
{
	if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return curr;

	SDL_Scancode sc = e->key.keysym.scancode;
	uint8_t pressed = (e->type == SDL_KEYDOWN);

	const Keymap *k = &gb->cfg.keymap;
	uint8_t mask = 0;
	if	(sc == k->right)	mask = JOYPAD_RIGHT;
	else if (sc == k->left)		mask = JOYPAD_LEFT;
	else if (sc == k->up)		mask = JOYPAD_UP;
	else if (sc == k->down)		mask = JOYPAD_DOWN;
	else if (sc == k->a)		mask = JOYPAD_A;
	else if (sc == k->b)		mask = JOYPAD_B;
	else if (sc == k->start)	mask = JOYPAD_START;
	else if (sc == k->select)	mask = JOYPAD_SELECT;
	else if (pressed && !e->key.repeat) handle_hotkey(gb, sc, 1);

	if (sc == k->turbo) {
		if (pressed) gb->cfg.turbo = 1;
		else gb->cfg.turbo = 0;
	}

	return pressed ? (curr | mask) : (curr & ~mask);
}

static void handle_gamepad_event (GB *gb, const SDL_Event *e)
{
	switch (e->type) {
		case SDL_CONTROLLERDEVICEADDED:
			open_gamepad(&gb->pad, e->cdevice.which);
			break;

		case SDL_CONTROLLERDEVICEREMOVED:
			if (gb->pad.connected && e->cdevice.which == gb->pad.instance_id) {
				cleanup_gamepad(&gb->pad);
				gb->joypad.pad_dpad = 0;
				gb->joypad.pad_stick = 0;
			}
			break;

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			uint8_t pressed = (e->type == SDL_CONTROLLERBUTTONDOWN);
			uint8_t mask = 0;
			switch (e->cbutton.button) {
				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: mask = JOYPAD_RIGHT; break;
				case SDL_CONTROLLER_BUTTON_DPAD_LEFT: mask = JOYPAD_LEFT; break;
				case SDL_CONTROLLER_BUTTON_DPAD_UP: mask = JOYPAD_UP; break;
				case SDL_CONTROLLER_BUTTON_DPAD_DOWN: mask = JOYPAD_DOWN; break;
				case SDL_CONTROLLER_BUTTON_A: mask = JOYPAD_A; break;
				case SDL_CONTROLLER_BUTTON_B: mask = JOYPAD_B; break;
				case SDL_CONTROLLER_BUTTON_START: mask = JOYPAD_START; break;
				case SDL_CONTROLLER_BUTTON_BACK: mask = JOYPAD_SELECT; break;
				default: return;
			}
			if (pressed) gb->joypad.pad_dpad |= mask;
			else gb->joypad.pad_dpad &= ~mask;
			break;
		}

		case SDL_CONTROLLERAXISMOTION:
			if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
				gb->joypad.pad_stick &= ~(JOYPAD_LEFT | JOYPAD_RIGHT);
				if (e->caxis.value < -GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_LEFT;
				else if (e->caxis.value > GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_RIGHT;
			} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
				gb->joypad.pad_stick &= ~(JOYPAD_UP | JOYPAD_DOWN);
				if (e->caxis.value < -GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_UP;
				else if (e->caxis.value > GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_DOWN;
			}
			break;
	}
}

int frontend_init (GB *gb, const char *game_title)
{
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	if (!init_screen(&gb->lcd, game_title)) {
		gb->running = 0;
		return 0;
	}

	init_gamepad(&gb->pad);

	if (!init_audio(&gb->audio, &gb->cfg)) {
		gb->running = 0;
		return 0;
	}
	return 1;
}

void frontend_shutdown (GB *gb)
{
	cleanup_audio(&gb->audio);
	cleanup_gamepad(&gb->pad);
	cleanup_screen(&gb->lcd);
	SDL_Quit();
}

int handle_events (GB *gb)
{
	SDL_Event e;

	while (SDL_PollEvent(&e)) {

		if (!handle_window_event(&e))
			return 0;

		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			if (gb->joypad.kb_buttons) {
				gb->joypad.kb_buttons = 0;
				joypad_update(gb, gb->joypad.pad_dpad | gb->joypad.pad_stick);
			}
			continue;
		}

		uint8_t prev_kb = gb->joypad.kb_buttons;
		uint8_t prev_pad = gb->joypad.pad_dpad | gb->joypad.pad_stick;

		gb->joypad.kb_buttons = handle_kb_event(gb, &e, gb->joypad.kb_buttons);
		handle_gamepad_event(gb, &e);

		uint8_t new_pad = gb->joypad.pad_dpad | gb->joypad.pad_stick;
		if (gb->joypad.kb_buttons != prev_kb || new_pad != prev_pad)
			joypad_update(gb, gb->joypad.kb_buttons | new_pad);
	}

	return 1;
}
