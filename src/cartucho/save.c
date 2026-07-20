#include "cartucho/save.h"
#include "gb.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static void *save_thread_fn (void *arg)
{
	GB *gb = (GB *) arg;
	const char *romfile = gb->save.romfile;
	Cartucho *cart = &gb->memory.cart;

	uint8_t *snap = NULL;
	if (cart->ram_size > 0) {
		snap = malloc(cart->ram_size);
		if (!snap) fprintf(stderr, "Autosave: not enough memory for RAM snapshot buffer\n");
	}

	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);

	while (atomic_load(&gb->save.thread_run)) {

		deadline.tv_sec  += AUTOSAVE_INTERVAL_MS / 1000;
		deadline.tv_nsec += (AUTOSAVE_INTERVAL_MS % 1000) * 1000000L;
		if (deadline.tv_nsec >= 1000000000L) {
			deadline.tv_sec++;
			deadline.tv_nsec -= 1000000000L;
		}

		pthread_mutex_lock(&gb->save.lock);
		(void) pthread_cond_timedwait(&gb->save.cond, &gb->save.lock, &deadline);
		pthread_mutex_unlock(&gb->save.lock);

		if (!atomic_load(&gb->save.thread_run)) break;
		if (!atomic_load(&gb->save.request)) continue;
		atomic_store(&gb->save.request, 0);

		uint8_t have_ram = (snap && cart->ram_size > 0);
		pthread_mutex_lock(&gb->save.lock);
		if (have_ram) memcpy(snap, cart->ram, cart->ram_size);

		uint8_t has_rtc = gb->memory.cart.has_rtc;
		RTC rtc_snap;
		if (has_rtc) rtc_snap = *((RTC *)cart->state);

		pthread_mutex_unlock(&gb->save.lock);
		save_sram_snapshot(cart, romfile, have_ram ? snap : NULL,
				have_ram ? cart->ram_size : 0, has_rtc ? &rtc_snap : NULL);
	}

	free(snap);
	return NULL;
}

void request_save (GB *gb)
{
	atomic_store(&gb->save.request, 1);
	pthread_mutex_lock(&gb->save.lock);
	pthread_cond_signal(&gb->save.cond);
	pthread_mutex_unlock(&gb->save.lock);
}

void start_save_thread (GB *gb, const char *romfile)
{
	if ((!gb->memory.cart.battery || !gb->memory.cart.ram_size) && !gb->memory.cart.has_rtc) return;
	if (atomic_load(&gb->save.thread_run)) return;

	gb->save.romfile = romfile;
	atomic_store(&gb->save.thread_run, 1);
	atomic_store(&gb->save.request, 0);

	if (pthread_create(&gb->save.thread, NULL, save_thread_fn, gb) != 0)
		atomic_store(&gb->save.thread_run, 0);
}

void stop_save_thread (GB *gb)
{
	if (!atomic_load(&gb->save.thread_run)) return;

	atomic_store(&gb->save.thread_run, 0);
	pthread_mutex_lock(&gb->save.lock);
	pthread_cond_signal(&gb->save.cond);
	pthread_mutex_unlock(&gb->save.lock);

	pthread_join(gb->save.thread, NULL);
}

