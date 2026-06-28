#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct Timer {
	uint16_t div;
	uint8_t tima;
	uint8_t tma;
	uint8_t tac;

	uint8_t tima_overflow;
	uint8_t reload;
} Timer;

int timer_step (Timer *timer);

#endif
