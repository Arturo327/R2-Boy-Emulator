#include "frontend/frontend.h"
#include "utils/utils.h"
#include "gb.h"

#include <stdio.h>
#include <stdlib.h>

#define JOYPAD_RIGHT 0x01
#define JOYPAD_LEFT 0x02
#define JOYPAD_UP 0x04
#define JOYPAD_DOWN 0x08
#define JOYPAD_A 0x10
#define JOYPAD_B 0x20
#define JOYPAD_SELECT 0x40
#define JOYPAD_START  0x80

#define GAMEPAD_DEADZONE 8000

#define SCREENSHOT_W 160
#define SCREENSHOT_H 144

static void take_screenshot (GB *gb)
{
	static int n = 0;

	char prefix[512];
	if (!path_with_suffix(gb->romfile, "_screenshot", prefix, sizeof(prefix))) {
		fprintf(stderr, "Screenshot: ROM path too long\n");
		return;
	}

	char path[600];
	if (!next_numbered_file(prefix, "bmp", &n, path, sizeof(path))) {
		fprintf(stderr, "Screenshot: could not build a filename\n");
		return;
	}

	if (write_bmp(path, gb->ppu.framebuffer, SCREENSHOT_W, SCREENSHOT_H))
		printf("Screenshot saved: %s\n", path);
	else
		fprintf(stderr, "Screenshot: could not save %s\n", path);
}

static int handle_window_event (SDL_Event *e)
{
	if (e->type == SDL_QUIT) return 0;
	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_CLOSE)
		return 0;
	return 1;
}

static int keybind_match (const Keybind *kb, SDL_Scancode sc, uint16_t mods, int is_release)
{
	if (kb->scancode != sc) return 0;
	if (kb->mods == 0 || is_release) return 1;

	uint16_t norm = 0;
	if (mods & KBMOD_CTRL) norm |= KBMOD_CTRL;
	if (mods & KBMOD_SHIFT) norm |= KBMOD_SHIFT;
	if (mods & KBMOD_ALT) norm |= KBMOD_ALT;
	if (mods & KBMOD_GUI) norm |= KBMOD_GUI;

	return norm == kb->mods;
}

static void toggle_pause (GB *gb)
{
	if (!gb->on) return;
	gb->paused = !gb->paused;
	if (gb->paused) {
		ring_clear(&gb->audio.ring);
		gb->apu.buffer_pos = 0;
	}
	fprintf(stderr, "%s\n", gb->paused ? "Paused" : "Resumed");
}

static void toggle_on (GB *gb)
{
	if (gb->on) {
		gb->on = 0;
		if (gb->paused) {
			gb->paused = 0;
			fprintf(stderr, "Resumed\n");
		}
		if (gb->audio.dev) {
			ring_clear(&gb->audio.ring);
			gb->apu.buffer_pos = 0;
		}
		if (gb->link.active) {
			ring_reset(&gb->link.rx);
			ring_reset(&gb->link.tx);
		}
		if (gb->memory.cart.has_rumble) {
			gb->memory.cart.rumble_on = 0;
			gb->memory.cart.rumble_on_ticks = 0;
			gb->memory.cart.rumble_since = 0;
			update_rumble(&gb->pad, 0);
		}
		shutdown_screen(&gb->ppu);
	} else {
		reset_gb(gb);
	}
}

static void toggle_mute (Config *cfg)
{
	uint8_t m = !atomic_load(&cfg->muted);
	atomic_store(&cfg->muted, m);
	fprintf(stderr, "Audio: %s\n", m ? "muted" : "unmuted");
}

static void adjust_turbo (Config *cfg, int delta)
{
	int v = cfg->turbo_speed + delta;
	if (v > 16) v = 16;
	if (v < 2) v = 2;
	cfg->turbo_speed = v;
	fprintf(stderr, "Turbo Speed: x%d\n", v);
}

static void adjust_volume (Config *cfg, int delta)
{
	int v = atomic_load(&cfg->volume) + delta;
	if (v > 100) v = 100;
	if (v < 0) v = 0;
	atomic_store(&cfg->volume, v);
	fprintf(stderr, "Volume: %d%%\n", v);
}

static void cycle_palette (GB *gb)
{
	DmgPalette pal = (DmgPalette)((gb->cfg.palette + 1) % PAL_COUNT);
	gb->cfg.palette = pal;
	gb->ppu.palette = pal;
	fprintf(stderr, "Palette: %s\n", palette_name(gb->cfg.palette));
}

static inline void save_state_1 (GB *gb) {
	gb->state_save_pending = 1;
	gb->state_num = 1;
}
static inline void load_state_1 (GB *gb) {
	gb->state_load_pending = 1;
	gb->state_num = 1;
}
static inline void save_state_2 (GB *gb) {
	gb->state_save_pending = 1;
	gb->state_num = 2;
}
static inline void load_state_2 (GB *gb) {
	gb->state_load_pending = 1;
	gb->state_num = 2;
}

static uint8_t kb_scancode_to_tilt_mask (const Keymap *k, SDL_Scancode sc, uint16_t mods, int is_release)
{
	static const Action actions[] = { ACT_TILT_RIGHT, ACT_TILT_LEFT, ACT_TILT_UP, ACT_TILT_DOWN };
	static const uint8_t masks[] = { TILT_RIGHT, TILT_LEFT, TILT_UP, TILT_DOWN };
	int n = sizeof(actions) / sizeof(actions[0]);
	for (int i = 0; i < n; i++) {
		Keybind kb = kb_binding(k, actions[i]);
		if (keybind_match(&kb, sc, mods, is_release)) return masks[i];
	}
	return 0;
}

static uint8_t kb_scancode_to_joypad_mask (const Keymap *k, SDL_Scancode sc, uint16_t mods, int is_release)
{
	static const Action actions[] = {
		ACT_RIGHT, ACT_LEFT, ACT_UP, ACT_DOWN, ACT_A, ACT_B, ACT_START, ACT_SELECT
	};
	static const uint8_t masks[] = {
		JOYPAD_RIGHT, JOYPAD_LEFT, JOYPAD_UP, JOYPAD_DOWN,
		JOYPAD_A, JOYPAD_B, JOYPAD_START, JOYPAD_SELECT
	};
	int n = sizeof(actions) / sizeof(actions[0]);
	for (int i = 0; i < n; i++) {
		Keybind kb = kb_binding(k, actions[i]);
		if (keybind_match(&kb, sc, mods, is_release)) return masks[i];
	}
	return 0;
}

static void handle_hotkey (GB *gb, SDL_Scancode sc, uint16_t mods, int pressed)
{
	if (!pressed) return;
	Config *cfg = &gb->cfg;
	if (keybind_match(&cfg->keymap.mute, sc, mods, 0))		toggle_mute(&gb->cfg);
	else if (keybind_match(&cfg->keymap.vol_up, sc, mods, 0))	adjust_volume(&gb->cfg, 10);
	else if (keybind_match(&cfg->keymap.vol_down, sc, mods, 0))	adjust_volume(&gb->cfg, -10);
	else if (keybind_match(&cfg->keymap.turbo_up, sc, mods, 0))	adjust_turbo(&gb->cfg, 1);
	else if (keybind_match(&cfg->keymap.turbo_down, sc, mods, 0))	adjust_turbo(&gb->cfg, -1);
	else if (keybind_match(&cfg->keymap.palette, sc, mods, 0))	cycle_palette(gb);
	else if (keybind_match(&cfg->keymap.save_1, sc, mods, 0))	save_state_1(gb);
	else if (keybind_match(&cfg->keymap.load_1, sc, mods, 0))	load_state_1(gb);
	else if (keybind_match(&cfg->keymap.save_2, sc, mods, 0))	save_state_2(gb);
	else if (keybind_match(&cfg->keymap.load_2, sc, mods, 0))	load_state_2(gb);
	else if (keybind_match(&cfg->keymap.screenshot, sc, mods, 0))	take_screenshot(gb);
	else if (keybind_match(&cfg->keymap.on, sc, mods, 0))		toggle_on(gb);
	else if (keybind_match(&cfg->keymap.pause, sc, mods, 0))	toggle_pause(gb);
	else if (keybind_match(&cfg->keymap.turbo, sc, mods, 0))	gb->cfg.turbo ^= 1;
}

static uint8_t handle_kb_event (GB *gb, const SDL_Event *e, uint8_t curr)
{
	if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return curr;

	SDL_Scancode sc = e->key.keysym.scancode;
	uint16_t mods = e->key.keysym.mod & KBMOD_ANY;
	uint8_t pressed = (e->type == SDL_KEYDOWN);
	int released = !pressed;

	const Keymap *k = &gb->cfg.keymap;
	uint8_t mask = kb_scancode_to_joypad_mask(k, sc, mods, released);
	if (pressed && !e->key.repeat) handle_hotkey(gb, sc, mods, 1);

	if (gb->cfg.turbo_hold && keybind_match(&k->turbo, sc, mods, released))
		gb->cfg.turbo = pressed ? 1 : 0;

	uint8_t tilt_mask = kb_scancode_to_tilt_mask(k, sc, mods, released);
	if (tilt_mask) {
		if (pressed) gb->joypad.kb_tilt |= tilt_mask;
		else gb->joypad.kb_tilt &= ~tilt_mask;
	}

	return pressed ? (curr | mask) : (curr & ~mask);
}

static uint8_t pad_button_to_joypad_mask (Padmap *p, SDL_GameControllerButton b)
{
	if (b == pad_binding(p, ACT_RIGHT))	return JOYPAD_RIGHT;
	if (b == pad_binding(p, ACT_LEFT))	return JOYPAD_LEFT;
	if (b == pad_binding(p, ACT_UP))	return JOYPAD_UP;
	if (b == pad_binding(p, ACT_DOWN))	return JOYPAD_DOWN;
	if (b == pad_binding(p, ACT_A))		return JOYPAD_A;
	if (b == pad_binding(p, ACT_B))		return JOYPAD_B;
	if (b == pad_binding(p, ACT_START))	return JOYPAD_START;
	if (b == pad_binding(p, ACT_SELECT))	return JOYPAD_SELECT;
	return 0;
}

static void handle_turbo (Config *cfg)
{
	if (cfg->turbo_hold) cfg->turbo = 1;
	else cfg->turbo ^= 1;
}

static void handle_gamepad_button (GB *gb, const SDL_Event *e)
{
	Padmap *pad = &gb->cfg.padmap;
	uint8_t pressed = (e->type == SDL_CONTROLLERBUTTONDOWN);
	SDL_GameControllerButton b = e->cbutton.button;
	uint8_t mask = pad_button_to_joypad_mask(pad, b);
	if (mask) {
		if (pressed) gb->joypad.pad_dpad |= mask;
		else gb->joypad.pad_dpad &= ~mask;
		return;
	}
	if (pressed) {
		if	(b == pad_binding(pad, ACT_TURBO))	handle_turbo(&gb->cfg);
		else if (b == pad_binding(pad, ACT_MUTE))	toggle_mute(&gb->cfg);
		else if (b == pad_binding(pad, ACT_VOL_UP))	adjust_volume(&gb->cfg, 10);
		else if (b == pad_binding(pad, ACT_VOL_DOWN))	adjust_volume(&gb->cfg, -10);
		else if (b == pad_binding(pad, ACT_TURBO_UP))	adjust_turbo(&gb->cfg, 1);
		else if (b == pad_binding(pad, ACT_TURBO_DOWN))	adjust_turbo(&gb->cfg, -1);
		else if (b == pad_binding(pad, ACT_PALETTE))	cycle_palette(gb);
		else if (b == pad_binding(pad, ACT_SAVE1))	save_state_1(gb);
		else if (b == pad_binding(pad, ACT_LOAD1))	load_state_1(gb);
		else if (b == pad_binding(pad, ACT_SAVE2))	save_state_2(gb);
		else if (b == pad_binding(pad, ACT_LOAD2))	load_state_2(gb);
		else if (b == pad_binding(pad, ACT_SCREENSHOT))	take_screenshot(gb);
		else if (b == pad_binding(pad, ACT_ON))		toggle_on(gb);
		else if (b == pad_binding(pad, ACT_PAUSE))	toggle_pause(gb);
	}
	if (!pressed && b == pad_binding(pad, ACT_TURBO)) gb->cfg.turbo = 0;
	return;
}

static void handle_gamepad_joypad (GB *gb, const SDL_Event *e)
{
	if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
		gb->joypad.pad_stick &= ~(JOYPAD_LEFT | JOYPAD_RIGHT);
		if (e->caxis.value < -GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_LEFT;
		else if (e->caxis.value > GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_RIGHT;

	} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
		gb->joypad.pad_stick &= ~(JOYPAD_UP | JOYPAD_DOWN);
		if (e->caxis.value < -GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_UP;
		else if (e->caxis.value > GAMEPAD_DEADZONE) gb->joypad.pad_stick |= JOYPAD_DOWN;

	} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX) {
		gb->joypad.pad_tilt_x = (e->caxis.value > -GAMEPAD_DEADZONE && e->caxis.value < GAMEPAD_DEADZONE)
			? 0 : e->caxis.value;

	} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) {
		gb->joypad.pad_tilt_y = (e->caxis.value > -GAMEPAD_DEADZONE && e->caxis.value < GAMEPAD_DEADZONE)
			? 0 : e->caxis.value;
	}
}

static void handle_gamepad_event (GB *gb, const SDL_Event *e)
{
	switch (e->type)
	{
	case SDL_CONTROLLERDEVICEADDED:
		open_gamepad(&gb->pad, e->cdevice.which);
		break;

	case SDL_CONTROLLERDEVICEREMOVED:
		if (gb->pad.connected && e->cdevice.which == gb->pad.instance_id) {
			cleanup_gamepad(&gb->pad);
			gb->joypad.pad_dpad = 0;
			gb->joypad.pad_stick = 0;
			gb->joypad.pad_tilt_x = 0;
			gb->joypad.pad_tilt_y = 0;
		}
		break;

	case SDL_CONTROLLERBUTTONDOWN: case SDL_CONTROLLERBUTTONUP:
		handle_gamepad_button(gb, e);
		break;

	case SDL_CONTROLLERAXISMOTION:
		handle_gamepad_joypad(gb, e);
		break;
	}
}

int frontend_init (GB *gb, const char *game_title)
{
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO
				| SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	if (!init_screen(&gb->lcd, game_title)) {
		gb->running = 0;
		return 0;
	}

	init_gamepad(&gb->pad);
	if (gb->memory.cart.mbc_type == MBC7) {
		printf("MBC7: %s\n", gb->pad.has_acel
			? "using real gamepad accelerometer"
			: "using accelerometer simulation via keyboard / right Joystick");
	}

	if (gb->memory.cart.mbc_type == CAM) {
		printf("Camera: %s\n", init_webcam(&gb->webcam)
			? "webcam connected"
			: "no usable webcam found, using a synthetic image");
		}

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
	if (gb->memory.cart.mbc_type == CAM)
		cleanup_webcam(&gb->webcam);
	SDL_Quit();
}

int handle_events (GB *gb)
{
	SDL_Event e;

	while (SDL_PollEvent(&e)) {

		if (!handle_window_event(&e))
			return 0;

		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
			uint8_t had_kb = gb->joypad.kb_buttons;
			gb->joypad.kb_buttons = 0;
			gb->joypad.kb_tilt = 0;
			gb->cfg.turbo = 0;
			if (had_kb)
				joypad_update(gb, gb->joypad.pad_dpad | gb->joypad.pad_stick);
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
