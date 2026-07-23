#include "frontend/frontend.h"
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

static void screenshot_path_prefix (const char *romfile, char *out, size_t outsize)
{
	size_t len = strlen(romfile);
	const char *dot = strrchr(romfile, '.');
	const char *slash = strrchr(romfile, '/');

	size_t base_len = len;
	if (dot && (!slash || dot > slash))
		base_len = (size_t)(dot - romfile);

	if (base_len > outsize - 1)
		base_len = outsize - 1;

	memcpy(out, romfile, base_len);
	out[base_len] = '\0';
}

static int file_exists (const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	fclose(f);
	return 1;
}

static int bmp_row_pad (int w) {
	int row_bytes = w * 3;
	return (4 - (row_bytes % 4)) % 4;
}

static uint32_t bmp_data_size (int w, int h) {
	return (uint32_t)(w * 3 + bmp_row_pad(w)) * (uint32_t)h;
}

static void write_bmp_row (FILE *f, const uint32_t *row, int w, int pad)
{
	static const uint8_t padbuf[3] = { 0, 0, 0 };

	for (int x = 0; x < w; x++) {
		uint8_t bgr[3] = {
			(uint8_t)(row[x] & 0xFF),
			(uint8_t)((row[x] >> 8) & 0xFF),
			(uint8_t)((row[x] >> 16) & 0xFF)
		};
		fwrite(bgr, 1, 3, f);
	}
	if (pad) fwrite(padbuf, 1, (size_t)pad, f);
}

static void write_bmp_rows (FILE *f, const uint32_t *pixels, int w, int h)
{
	int pad = bmp_row_pad(w);
	const uint32_t *row = pixels + (size_t)(h - 1) * w;

	for (int y = 0; y < h; y++) {
		write_bmp_row(f, row, w, pad);
		row -= w;
	}
}

static void write_bmp_header (FILE *f, int w, int h, uint32_t data_size)
{
	uint32_t file_size = 54u + data_size;
	uint8_t header[54];
	memset(header, 0, sizeof(header));

	header[0] = 'B';
	header[1] = 'M';
	header[2] = (uint8_t)(file_size);
	header[3] = (uint8_t)(file_size >> 8);
	header[4] = (uint8_t)(file_size >> 16);
	header[5] = (uint8_t)(file_size >> 24);
	header[10] = 54;
	header[14] = 40;
	header[18] = (uint8_t)(w);
	header[19] = (uint8_t)(w >> 8);
	header[20] = (uint8_t)(w >> 16);
	header[21] = (uint8_t)(w >> 24);
	header[22] = (uint8_t)(h);
	header[23] = (uint8_t)(h >> 8);
	header[24] = (uint8_t)(h >> 16);
	header[25] = (uint8_t)(h >> 24);
	header[26] = 1;
	header[28] = 24;
	header[34] = (uint8_t)(data_size);
	header[35] = (uint8_t)(data_size >> 8);
	header[36] = (uint8_t)(data_size >> 16);
	header[37] = (uint8_t)(data_size >> 24);

	fwrite(header, 1, sizeof(header), f);
}

static void take_screenshot (GB *gb)
{
	char prefix[512];
	screenshot_path_prefix(gb->romfile, prefix, sizeof(prefix));

	char path[600];
	int n = 0;
	do {
		n++;
		snprintf(path, sizeof(path), "%s_screenshot_%03d.bmp", prefix, n);
	} while (file_exists(path));

	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "Screenshot: could not save %s\n", path);
		return;
	}

	write_bmp_header(f, SCREENSHOT_W, SCREENSHOT_H,
			bmp_data_size(SCREENSHOT_W, SCREENSHOT_H));
	write_bmp_rows(f, gb->ppu.framebuffer, SCREENSHOT_W, SCREENSHOT_H);

	fclose(f);
	printf("Screenshot saved: %s\n", path);
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

	return (norm & kb->mods) == kb->mods;
}

static void toggle_mute (Config *cfg)
{
	uint8_t m = !atomic_load(&cfg->muted);
	atomic_store(&cfg->muted, m);
	fprintf(stderr, "Audio: %s\n", m ? "muted" : "unmuted");
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
	static const uint8_t masks[]  = { TILT_RIGHT, TILT_LEFT, TILT_UP, TILT_DOWN };
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
	else if (keybind_match(&cfg->keymap.palette, sc, mods, 0))	cycle_palette(gb);
	else if (keybind_match(&cfg->keymap.save_1, sc, mods, 0))	save_state_1(gb);
	else if (keybind_match(&cfg->keymap.load_1, sc, mods, 0))	load_state_1(gb);
	else if (keybind_match(&cfg->keymap.save_2, sc, mods, 0))	save_state_2(gb);
	else if (keybind_match(&cfg->keymap.load_2, sc, mods, 0))	load_state_2(gb);
	else if (keybind_match(&cfg->keymap.screenshot, sc, mods, 0))	take_screenshot(gb);
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

	if (keybind_match(&k->turbo, sc, mods, released))
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
		if	(b == pad_binding(pad, ACT_TURBO))	gb->cfg.turbo = 1;
		else if (b == pad_binding(pad, ACT_MUTE))	toggle_mute(&gb->cfg);
		else if (b == pad_binding(pad, ACT_VOL_UP))	adjust_volume(&gb->cfg, 10);
		else if (b == pad_binding(pad, ACT_VOL_DOWN))	adjust_volume(&gb->cfg, -10);
		else if (b == pad_binding(pad, ACT_PALETTE))	cycle_palette(gb);
		else if (b == pad_binding(pad, ACT_SAVE1))	save_state_1(gb);
		else if (b == pad_binding(pad, ACT_LOAD1))	load_state_1(gb);
		else if (b == pad_binding(pad, ACT_SAVE2))	save_state_2(gb);
		else if (b == pad_binding(pad, ACT_LOAD2))	load_state_2(gb);
		else if (b == pad_binding(pad, ACT_SCREENSHOT))	take_screenshot(gb);
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
			if (gb->joypad.kb_buttons) {
				gb->joypad.kb_buttons = 0;
				joypad_update(gb, gb->joypad.pad_dpad | gb->joypad.pad_stick);
				gb->cfg.turbo = 0;
				gb->joypad.kb_tilt = 0;
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
