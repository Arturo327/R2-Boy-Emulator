#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>
#include <pthread.h>

#define AUTOSAVE_INTERVAL_MS 5000

typedef struct GB GB;
typedef struct Cartucho Cartucho;

typedef struct AutoSave {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	pthread_t thread;
	const char *romfile;
	_Atomic uint8_t request;
	_Atomic uint8_t thread_run;
} AutoSave;

void request_save (GB *gb);
void start_save_thread (GB *gb, const char *romfile);
void stop_save_thread (GB *gb);
int load_sram (Cartucho *cart, const char *romfile);
int save_sram (Cartucho *cart, const char *romfile);

#endif
