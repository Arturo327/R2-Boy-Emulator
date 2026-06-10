#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

typedef struct Timer {
	uint8_t div;
    	uint8_t tima;
    	uint8_t tma;
    	uint8_t tac;

    	uint16_t div_counter;
    	uint16_t tima_counter;
} Timer;

int timer_step (Timer *timer, int cycles);

#endif
