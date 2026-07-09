#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <stdint.h>
#include <SDL2/SDL.h>

typedef struct Gamepad {
	SDL_GameController *ctrl;
	SDL_JoystickID instance_id;
	uint8_t connected;
	uint8_t has_rumble;
} Gamepad;

int init_gamepad (Gamepad *pad);
void cleanup_gamepad (Gamepad *pad);
void update_rumble (Gamepad *pad, uint8_t active);
void open_gamepad (Gamepad *pad, int device_index);

#endif
