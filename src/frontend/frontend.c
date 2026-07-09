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

static int handle_window_event (const SDL_Event *e)
{
	if (e->type == SDL_QUIT) return 0;
	if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_CLOSE)
		return 0;
	return 1;
}

static uint8_t handle_kb_event (const SDL_Event *e, uint8_t curr)
{
	if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return curr;

	uint8_t pressed = (e->type == SDL_KEYDOWN);
	uint8_t mask = 0;

	switch (e->key.keysym.scancode) {
		case SDL_SCANCODE_RIGHT: mask = JOYPAD_RIGHT; break;
		case SDL_SCANCODE_LEFT: mask = JOYPAD_LEFT; break;
		case SDL_SCANCODE_UP: mask = JOYPAD_UP; break;
		case SDL_SCANCODE_DOWN: mask = JOYPAD_DOWN; break;
		case SDL_SCANCODE_X: mask = JOYPAD_A; break;
		case SDL_SCANCODE_Z: mask = JOYPAD_B; break;
		case SDL_SCANCODE_RETURN: mask = JOYPAD_START; break;
		case SDL_SCANCODE_BACKSPACE: mask = JOYPAD_SELECT; break;
		default: break;
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

int frontend_init (GB *gb)
{
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	if (!init_screen(&gb->lcd)) {
		gb->running = 0;
		return 0;
	}

	init_gamepad(&gb->pad);

	if (!init_audio(&gb->audio)) {
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

		gb->joypad.kb_buttons = handle_kb_event(&e, gb->joypad.kb_buttons);
		handle_gamepad_event(gb, &e);

		uint8_t new_pad = gb->joypad.pad_dpad | gb->joypad.pad_stick;
		if (gb->joypad.kb_buttons != prev_kb || new_pad != prev_pad)
			joypad_update(gb, gb->joypad.kb_buttons | new_pad);
	}

	return 1;
}
