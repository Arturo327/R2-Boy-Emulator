#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>
#include <SDL2/SDL.h>

typedef struct GB GB;

typedef struct LCD {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
} LCD;

typedef struct Audio {
	SDL_AudioDeviceID dev;
	uint32_t sample_rate;
} Audio;

int init_screen (LCD *lcd);
void cleanup_screen (LCD *lcd);
void update_screen (LCD *lcd, uint32_t *framebuffer);
int handle_events (GB *gb);

int init_audio (Audio *audio);
void cleanup_audio (Audio *audio);
void queue_audio (GB *gb);

#endif
