#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>
#include <SDL2/SDL.h>

#include "frontend/gamepad.h"
#include "frontend/audio.h"
#include "frontend/lcd.h"
#include "frontend/webcam.h"

typedef struct GB GB;

int frontend_init (GB *gb, const char *game_title);
void frontend_shutdown (GB *gb);
int handle_events (GB *gb);

#endif
