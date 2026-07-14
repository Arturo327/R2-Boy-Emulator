#include "frontend/audio.h"
#include "gb.h"

#include <string.h>
#include <stdio.h>

static void ring_push (AudioRing *r, int16_t *src, uint32_t n)
{
	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_relaxed);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_acquire);
	uint32_t free_space = AUDIO_RING_SIZE - ((wp - rp) & (AUDIO_RING_SIZE - 1));

	if (n > free_space) n = free_space;
	if (n == 0) return;

	uint32_t idx = wp & (AUDIO_RING_SIZE - 1);
	uint32_t first = AUDIO_RING_SIZE - idx;
	if (first > n) first = n;

	memcpy(r->buffer + idx, src, first * sizeof(int16_t));
	if (n > first)
		memcpy(r->buffer, src + first, (n - first) * sizeof(int16_t));

	atomic_store_explicit(&r->write_pos, wp + n, memory_order_release);
}

static void audio_callback (void *userdata, uint8_t *stream, int len)
{
	Audio *audio = (Audio *)userdata;
	AudioRing *r = &audio->ring;
	int16_t *out = (int16_t *)stream;
	int n = len / sizeof(int16_t);

	uint32_t wp = atomic_load_explicit(&r->write_pos, memory_order_acquire);
	uint32_t rp = atomic_load_explicit(&r->read_pos, memory_order_relaxed);
	uint32_t avail = (wp - rp) & (AUDIO_RING_SIZE - 1);
	int fill = avail < (uint32_t)n ? (int)avail : n;

	uint8_t target = 100;
	uint8_t muted = 0;
	if (audio->cfg) {
		target = atomic_load_explicit(&audio->cfg->volume, memory_order_relaxed);
		muted  = atomic_load_explicit(&audio->cfg->muted,  memory_order_relaxed);
	}
	if (muted) target = 0;

	int from = audio->last_volume;
	for (int i = 0; i < fill; i++) {
		int v = fill > 1 ? from + (target - from) * i / (fill - 1) : target;
		out[i] = (int16_t)((int32_t)r->buffer[(rp + i) & (AUDIO_RING_SIZE - 1)] * v / 100);
	}
	audio->last_volume = target;

	if (fill < n)
		memset(out + fill, 0, (size_t)(n - fill) * sizeof(int16_t));

	atomic_store_explicit(&r->read_pos, rp + fill, memory_order_release);
}

int init_audio (Audio *audio, struct Config *cfg) {

	SDL_AudioSpec want = {0}, have;
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 256;
	want.callback = audio_callback;
	want.userdata = audio;
	audio->cfg = cfg;

	audio->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

	if (!audio->dev) {
		fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
		return 0;
	}

	audio->sample_rate = have.freq;
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
