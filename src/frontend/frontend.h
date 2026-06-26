#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdint.h>
#include <SDL2/SDL.h>
#include <stdatomic.h>

#define AUDIO_RING_SIZE 8192

_Static_assert((AUDIO_RING_SIZE & (AUDIO_RING_SIZE - 1)) == 0,
	       "AUDIO_RING_SIZE must be power of 2");

typedef struct GB GB;

typedef struct LCD {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
} LCD;

typedef struct AudioRing {
	int16_t buffer[AUDIO_RING_SIZE];
	_Atomic uint32_t write_pos;
	_Atomic uint32_t read_pos;
} AudioRing;

static inline uint32_t ring_used (AudioRing *r) {
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_acquire);
	return (wp - rp) & (AUDIO_RING_SIZE - 1);
}

typedef struct Audio {
	SDL_AudioDeviceID dev;
	uint32_t sample_rate;
	AudioRing ring;
} Audio;

int init_screen (LCD *lcd);
void cleanup_screen (LCD *lcd);
void update_screen (LCD *lcd, uint32_t *framebuffer);
int handle_events (GB *gb);

int init_audio (Audio *audio);
void cleanup_audio (Audio *audio);
void queue_audio (GB *gb);

#endif
