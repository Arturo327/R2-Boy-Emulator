#include "frontend/gamepad.h"

#include <string.h>
#include <stdio.h>

void open_gamepad (Gamepad *pad, int device_index) {
	if (pad->connected) return;

	SDL_GameController *ctrl = SDL_GameControllerOpen(device_index);
	if (!ctrl) return;

	pad->ctrl = ctrl;
	pad->instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(ctrl));
	pad->connected = 1;

#if SDL_VERSION_ATLEAST(2, 0, 9)
	pad->has_rumble = (SDL_GameControllerRumble(ctrl, 0, 0, 1) == 0);
#endif


#if SDL_VERSION_ATLEAST(2, 0, 14)
	pad->has_acel = SDL_GameControllerHasSensor(ctrl, SDL_SENSOR_ACCEL);
	if (pad->has_acel && SDL_GameControllerSetSensorEnabled(ctrl, SDL_SENSOR_ACCEL, SDL_TRUE) != 0)
		pad->has_acel = 0;
#endif

	printf("Connected Gamepad: %s%s%s\n", SDL_GameControllerName(ctrl),
		pad->has_rumble ? " [rumble]" : "",
		pad->has_acel  ? " [acelerometer]" : "");
}

int gamepad_get_acel (Gamepad *pad, float out[3])
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	if (!pad->connected || !pad->has_acel) return 0;
	return SDL_GameControllerGetSensorData(pad->ctrl, SDL_SENSOR_ACCEL, out, 3) == 0;
#else
	(void)pad;
	(void)out;
	return 0;
#endif
}

int init_gamepad (Gamepad *pad)
{
	memset(pad, 0, sizeof(Gamepad));
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			open_gamepad(pad, i);
			break;
		}
	}
	if (!pad->connected)
		printf("No gamepad detected\n");
	return pad->connected;
}

void cleanup_gamepad (Gamepad *pad)
{
#if SDL_VERSION_ATLEAST(2, 0, 14)
	if (pad->connected && pad->has_acel)
		SDL_GameControllerSetSensorEnabled(pad->ctrl, SDL_SENSOR_ACCEL, SDL_FALSE);
#endif
	if (pad->ctrl) SDL_GameControllerClose(pad->ctrl);
	memset(pad, 0, sizeof(Gamepad));
}

void update_rumble (Gamepad *pad, uint8_t active)
{
#if SDL_VERSION_ATLEAST(2, 0, 9)
	if (!pad->connected || !pad->has_rumble) return;
	if (active == pad->rumble_active) return;
	pad->rumble_active = active;
	if (active) SDL_GameControllerRumble(pad->ctrl, 0xFFFF, 0xFFFF, 50);
	else SDL_GameControllerRumble(pad->ctrl, 0, 0, 0);
#else
	(void)pad;
	(void)active;
#endif
}
