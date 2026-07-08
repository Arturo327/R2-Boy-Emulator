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

int init_screen (LCD *lcd) {

	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
		return 0;
	}

	lcd->window = SDL_CreateWindow(
		"R2-Boy",
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

	srand((unsigned)time(NULL));
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

void update_screen (LCD *lcd, uint32_t *framebuffer, uint8_t rumble) {
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

void ring_push (AudioRing *r, int16_t *src, uint32_t n)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_relaxed);
	for (uint32_t i = 0; i < n; i++)
		r->buffer[(wp + i) & (AUDIO_RING_SIZE - 1)] = src[i];
	atomic_store_explicit(&r->write_pos, wp + n, memory_order_release);
}

static void audio_callback (void *userdata, uint8_t *stream, int len)
{
	AudioRing *r = (AudioRing *)userdata;
	int16_t *out = (int16_t *)stream;
	int n = len / sizeof(int16_t);

	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_relaxed);
	uint32_t avail = (wp - rp) & (AUDIO_RING_SIZE - 1);
	int fill = avail < (uint32_t)n ? (int)avail : n;

	for (int i = 0; i < fill; i++)
		out[i] = r->buffer[(rp + i) & (AUDIO_RING_SIZE - 1)];

	if (fill < n) {
		memset(out + fill, 0, (n - fill) * sizeof(int16_t));
	}

	atomic_store_explicit(&r->read_pos, rp + fill, memory_order_release);
}

int init_audio (Audio *audio) {

	SDL_AudioSpec want = {0}, have;
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 256;
	want.callback = audio_callback;
	want.userdata = &audio->ring;

	audio->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

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
	if (!gb->audio.dev || gb->apu.buffer_pos == 0) return;
	ring_push(&gb->audio.ring, gb->apu.buffer, gb->apu.buffer_pos);
	gb->apu.buffer_pos = 0;
}
