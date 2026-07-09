#include "frontend/frontend.h"
#include "gb.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static void handle_gamepad_event (GB *gb, const SDL_Event *e, uint8_t *pad_buttons)
{
	switch (e->type) {
		case SDL_CONTROLLERDEVICEADDED:
			open_gamepad(&gb->pad, e->cdevice.which);
			break;

		case SDL_CONTROLLERDEVICEREMOVED:
			if (gb->pad.connected && e->cdevice.which == gb->pad.instance_id) {
				printf("Gamepad Disconnected\n");
				cleanup_gamepad(&gb->pad);
				*pad_buttons = 0;
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
			*pad_buttons = pressed ? (*pad_buttons | mask) : (*pad_buttons & ~mask);
			break;
		}

		case SDL_CONTROLLERAXISMOTION:
			if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
				*pad_buttons &= ~(JOYPAD_LEFT | JOYPAD_RIGHT);
				if (e->caxis.value < -GAMEPAD_DEADZONE) *pad_buttons |= JOYPAD_LEFT;
				else if (e->caxis.value > GAMEPAD_DEADZONE) *pad_buttons |= JOYPAD_RIGHT;
			} else if (e->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
				*pad_buttons &= ~(JOYPAD_UP | JOYPAD_DOWN);
				if (e->caxis.value < -GAMEPAD_DEADZONE) *pad_buttons |= JOYPAD_UP;
				else if (e->caxis.value > GAMEPAD_DEADZONE) *pad_buttons |= JOYPAD_DOWN;
			}
			break;
	}
}

int handle_events (GB *gb)
{
	SDL_Event e;
	static uint8_t kb_buttons = 0;
	static uint8_t pad_buttons = 0;

	while (SDL_PollEvent(&e)) {
		if (!handle_window_event(&e))
			return 0;

		uint8_t prev_kb = kb_buttons;
		uint8_t prev_pad = pad_buttons;

		kb_buttons = handle_kb_event(&e, kb_buttons);
		handle_gamepad_event(gb, &e, &pad_buttons);

		if (kb_buttons != prev_kb || pad_buttons != prev_pad)
			joypad_update(gb, kb_buttons | pad_buttons);
	}

	return 1;
}
