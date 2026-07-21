#include "cartucho/save.h"
#include "gb.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static void make_sav_path (const char *romfile, char *out, size_t outsize) {
	size_t len = strlen(romfile);
	const char *dot = strrchr(romfile, '.');
	const char *slash = strrchr(romfile, '/');

	size_t base_len = len;
	if (dot && (!slash || dot > slash)) {
		base_len = (size_t)(dot - romfile);
	}

	if (base_len > outsize - 5) base_len = outsize - 5;

	memcpy(out, romfile, base_len);
	memcpy(out + base_len, ".sav", 5);
}

static int read_int64 (FILE *f, int64_t *v)
{
	*v = 0;
	for (int i = 0; i < 8; i++) {
		int a = fgetc(f);
		if (a == EOF) return 0;
		*v |= (int64_t)((uint64_t)(uint8_t)a) << (i * 8);
	}
	return 1;
}

static int write_int64 (FILE *f, int64_t v)
{
	for (int i = 0; i < 8; i++) {
		uint8_t a = (v >> (i * 8)) & 0xFF;
		if (fputc(a, f) == EOF) return 0;
	}
	return 1;
}

static void load_rtc_data (RTC *rtc, uint8_t *buf, int64_t base)
{
	rtc->s = buf[0] & 0x3F;
	rtc->m = buf[1] & 0x3F;
	rtc->h = buf[2] & 0x1F;
	rtc->d = ((uint16_t)(buf[4] & 0x01) << 8) | buf[3];
	rtc->halt = (buf[4] & 0x40) != 0;
	rtc->carry = (buf[4] & 0x80) != 0;
	rtc->base = (time_t) base;

	rtc->s_l = rtc->s;
	rtc->m_l = rtc->m;
	rtc->h_l = rtc->h;
	rtc->d_l = rtc->d;
	rtc->halt_l = rtc->halt;
	rtc->carry_l = rtc->carry;
}

int load_sram (Cartucho *cart, const char *romfile)
{
	if (!cart->battery) return 0;
	if ((!cart->ram || cart->ram_size == 0) &&
		!cart->has_rtc && cart->mbc_type != HUC3) return 0;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));

	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	if (cart->ram && cart->ram_size > 0) {
		size_t a = fread(cart->ram, 1, cart->ram_size, f);
		if (a != cart->ram_size) {
			fprintf(stderr, "Could not load saved game %s\n", path);
			fclose(f);
			return 0;
		}
	}

	if (cart->has_rtc) {
		uint8_t buf[5];
		int64_t base;

		if (fread(buf, 1, 5, f) == 5 && read_int64(f, &base)) {
			load_rtc_data((RTC *)cart->state, buf, base);
		} else {
			printf("No RTC data in %s, starting clock fresh\n", path);
		}
	}

	if (cart->mbc_type == HUC3) {
		HuC3State *h = (HuC3State *)cart->state;
		if (fread(h->mem, 1, 256, f) != 256 || !read_int64(f, &h->base)) {
			printf("No RTC data in %s, starting clock fresh\n", path);
		}
		h->select = 0;
		h->cmd = 0;
		h->arg = 0;
		h->addr = 0;
		h->response = 0;
		h->ready = 1;
		h->ir = 0;
	}

	fclose(f);
	printf("Loaded game: %s\n", path);
	return 1;
}

static int write_sav_file (const char *savefile, const uint8_t *ram, uint32_t ram_size,
			const RTC *rtc, const void *extra, const size_t extra_size)
{
	char tmp_path[520];
	int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", savefile);
	if (n < 0 || (size_t)n >= sizeof(tmp_path)) {
		fprintf(stderr, "Save path too long: %s\n", savefile);
		return 0;
	}

	FILE *f = fopen(tmp_path, "wb");
	if (!f) goto fail;

	if (ram && ram_size > 0)
		if (fwrite(ram, 1, ram_size, f) != ram_size) goto fail;

	if (rtc) {
		uint8_t dh = ((rtc->d >> 8) & 0x01)
			| (rtc->halt  ? 0x40 : 0)
			| (rtc->carry ? 0x80 : 0);
		uint8_t buf[5] = { rtc->s, rtc->m, rtc->h,
				(uint8_t)(rtc->d & 0xFF), dh };
		int64_t base = (int64_t) rtc->base;

		if (fwrite(buf, 1, 5, f) != 5) goto fail;
		if (!write_int64(f, base)) goto fail;
	}

	if (extra && extra_size > 0)
		if (fwrite(extra, 1, extra_size, f) != extra_size) goto fail;

	if (fflush(f)) goto fail;
	if (fclose(f)) goto fail;
	if (rename(tmp_path, savefile)) goto fail;
	return 1;

fail:
	fprintf(stderr, "Could not save game\n");
	fclose(f);
	remove(tmp_path);
	return 0;
}

int save_sram (Cartucho *cart, const char *romfile)
{
	if (!cart->battery) return 0;
	if ((!cart->ram || cart->ram_size == 0)
		&& !cart->has_rtc && cart->mbc_type != HUC3) return 0;

	char savefile[512];
	make_sav_path(romfile, savefile, sizeof(savefile));

	uint8_t *extra = NULL;
	uint32_t extra_size = 0;
	uint8_t huc3_buf[264];
	if (cart->mbc_type == HUC3) {
		HuC3State *h = (HuC3State *)cart->state;
		memcpy(huc3_buf, h->mem, 256);
		int64_t base = h->base;
		for (int i = 0; i < 8; i++) {
			huc3_buf[256 + i] = (uint8_t)((base >> (i * 8)) & 0xFF);
		}
		extra = huc3_buf;
		extra_size = sizeof(huc3_buf);
	}

	int save_success = write_sav_file(savefile, cart->ram, cart->ram_size,
				cart->has_rtc ? cart->state : NULL, extra, extra_size);

	if (save_success) {
		printf("Saved game: %s\n", savefile);
		return 1;
	}
	return 0;
}

static void save_sram_snapshot (Cartucho *cart, const char *romfile,
			 const uint8_t *ram_snap, uint32_t ram_size, const RTC *rtc_snap,
			 const uint8_t *extra_snap, uint32_t extra_size)
{
	if (!cart->battery) return;
	if ((!ram_snap || ram_size == 0) &&
		!cart->has_rtc && !(extra_snap && extra_size)) return;

	char path[512];
	make_sav_path(romfile, path, sizeof(path));
	write_sav_file(path, ram_snap, ram_size,
		cart->has_rtc ? rtc_snap : NULL, extra_snap, extra_size);
}

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

		uint8_t is_huc3 = (cart->mbc_type == HUC3);
		HuC3State huc3_snap;
		uint8_t huc3_buf[264];
		if (is_huc3) {
			huc3_snap = *((HuC3State *)cart->state);
			memcpy(huc3_buf, huc3_snap.mem, 256);
			int64_t base = huc3_snap.base;
			for (int i = 0; i < 8; i++) {
				huc3_buf[256 + i] = (uint8_t)((base >> (i * 8)) & 0xFF);
			}
		}

		pthread_mutex_unlock(&gb->save.lock);
		save_sram_snapshot(cart, romfile, have_ram ? snap : NULL,
				have_ram ? cart->ram_size : 0,
				has_rtc ? &rtc_snap : NULL,
				is_huc3 ? (const uint8_t *)huc3_buf : NULL,
				is_huc3 ? (uint32_t) sizeof(huc3_buf) : 0);
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
	if ((!gb->memory.cart.battery || !gb->memory.cart.ram_size)
			&& !gb->memory.cart.has_rtc
			&& gb->memory.cart.mbc_type != HUC3) return;
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

