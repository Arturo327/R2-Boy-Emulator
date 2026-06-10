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

int init_screen (LCD *lcd);
void cleanup_screen (LCD *lcd);
void update_screen (LCD *lcd, uint32_t *framebuffer);
int handle_events (GB *gb);

#endif
