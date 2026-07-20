#ifndef SAVESTATE_H
#define SAVESTATE_H

#include <stdint.h>
#include <stdio.h>

#define SAVESTATE_MAGIC 0x59423252u

typedef struct GB GB;

typedef struct {
	uint32_t magic;
	uint32_t rom_size;
	char title[17];
} StateHeader;

typedef struct {
	FILE *f;
	int saving;
	int ok;
} SaveState;

int can_save_state (GB *gb);
int save_state (GB *gb);
int load_state (GB *gb);

#endif
