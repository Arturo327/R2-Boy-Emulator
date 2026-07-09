#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>
#include <SDL2/SDL.h>

#include "frontend/gamepad.h"
#include "frontend/audio.h"
#include "frontend/lcd.h"

typedef struct GB GB;

int handle_events (GB *gb);

#endif
