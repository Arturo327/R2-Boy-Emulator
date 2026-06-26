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

int init_screen (LCD *lcd) {

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	lcd->window = SDL_CreateWindow(
		"r2boy",
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
		lcd->window   = NULL;
		return 0;
	}

	SDL_RenderSetLogicalSize(lcd->renderer, 160, 144);
	SDL_SetRenderDrawColor(lcd->renderer, 0, 0, 0, 255);

	return 1;
}

void cleanup_screen (LCD *lcd) {
	if (lcd->texture) { SDL_DestroyTexture(lcd->texture); lcd->texture  = NULL; }
	if (lcd->renderer) { SDL_DestroyRenderer(lcd->renderer); lcd->renderer = NULL; }
	if (lcd->window) { SDL_DestroyWindow(lcd->window); lcd->window = NULL; }
	SDL_Quit();
}

void update_screen (LCD *lcd, uint32_t *framebuffer) {
	if (!lcd->texture || !lcd->renderer) return;

	SDL_UpdateTexture(lcd->texture, NULL, framebuffer, 160 * sizeof(uint32_t));
	SDL_RenderCopy(lcd->renderer, lcd->texture, NULL, NULL);
	SDL_RenderPresent(lcd->renderer);
}

int handle_events (GB *gb) {
	SDL_Event e;
	static uint8_t buttons = 0;

	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) return 0;
		if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
			return 0;

		if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			uint8_t pressed = (e.type == SDL_KEYDOWN) ? 1 : 0;
			uint8_t mask = 0;

			switch (e.key.keysym.scancode) {
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

			if (mask) {
				if (pressed) buttons |= mask;
				else buttons &= ~mask;
				joypad_update(gb, buttons);
			}
		}
	}
	return 1;
}

int init_audio (Audio *audio) {

	SDL_AudioSpec want = {0}, have;
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 512;

	audio->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

	if (!audio->dev) {
		fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
		return 0;
	}

	audio->sample_rate = have.freq;
	SDL_PauseAudioDevice(audio->dev, 0);

	return 1;
}

void cleanup_audio (Audio *audio) {
	if (audio->dev) {
		SDL_CloseAudioDevice(audio->dev);
		audio->dev = 0;
	}
}

void queue_audio (GB *gb) {
	if (!gb->audio.dev) return;
	if (gb->apu.buffer_pos == 0) return;

	SDL_QueueAudio(gb->audio.dev, gb->apu.buffer, gb->apu.buffer_pos * sizeof(int16_t));
	gb->apu.buffer_pos = 0;
}
