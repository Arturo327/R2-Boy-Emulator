#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <SDL2/SDL.h>
#include <stdatomic.h>

#define AUDIO_RING_SIZE 8192

_Static_assert((AUDIO_RING_SIZE & (AUDIO_RING_SIZE - 1)) == 0,
	       "AUDIO_RING_SIZE must be power of 2");

typedef struct GB GB;
struct Config;

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
	struct Config *cfg;
	int last_volume;
} Audio;

int init_audio (Audio *audio, struct Config *cfg);
void cleanup_audio (Audio *audio);
void queue_audio (GB *gb);

#endif
