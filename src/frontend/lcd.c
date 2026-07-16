#include "frontend/lcd.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int init_screen (LCD *lcd, const char *game_title)
{
	char final_title[26] = "R2-Boy - ";
	strcat(final_title, game_title);

	lcd->window = SDL_CreateWindow(
		final_title,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		160 * 4, 144 * 4,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
	);
	if (!lcd->window) {
		fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
		return 0;
	}

	lcd->renderer = SDL_CreateRenderer(
		lcd->window, -1,
		SDL_RENDERER_ACCELERATED
	);
	if (!lcd->renderer) {
		fprintf(stderr, "SDL_CreateRenderer error: %s, trying software\n", SDL_GetError());
		lcd->renderer = SDL_CreateRenderer(lcd->window, -1, SDL_RENDERER_SOFTWARE);
		if (!lcd->renderer) {
			fprintf(stderr, "Software renderer also failed: %s\n", SDL_GetError());
			SDL_DestroyWindow(lcd->window);
			lcd->window = NULL;
			return 0;
		}
	}

	lcd->texture = SDL_CreateTexture(
		lcd->renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		160, 144
	);
	if (!lcd->texture) {
		fprintf(stderr, "SDL_CreateTexture error: %s\n", SDL_GetError());
		SDL_DestroyRenderer(lcd->renderer);
		SDL_DestroyWindow(lcd->window);
		lcd->renderer = NULL;
		lcd->window = NULL;
		return 0;
	}

	srand((unsigned)time(NULL));
	SDL_RenderSetLogicalSize(lcd->renderer, 160, 144);
	SDL_SetRenderDrawColor(lcd->renderer, 0, 0, 0, 255);

	return 1;
}

void cleanup_screen (LCD *lcd)
{
	if (lcd->texture) { SDL_DestroyTexture(lcd->texture); lcd->texture  = NULL; }
	if (lcd->renderer) { SDL_DestroyRenderer(lcd->renderer); lcd->renderer = NULL; }
	if (lcd->window) { SDL_DestroyWindow(lcd->window); lcd->window = NULL; }
}

void update_screen (LCD *lcd, uint32_t *framebuffer, uint8_t rumble)
{
	if (!lcd->texture || !lcd->renderer) return;

	SDL_UpdateTexture(lcd->texture, NULL, framebuffer, 160 * sizeof(uint32_t));
	SDL_RenderClear(lcd->renderer);

	SDL_Rect dst = {0, 0, 160, 144};
	if (rumble) {
		dst.x = (rand() & 7) - 4;
		dst.y = (rand() & 7) - 4;
	}

	SDL_RenderCopy(lcd->renderer, lcd->texture, NULL, &dst);
	SDL_RenderPresent(lcd->renderer);
}
