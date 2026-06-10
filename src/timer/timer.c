#include "timer/timer.h"

int timer_step (Timer *timer, int cycles) {
	int hay_interrupt = 0;

	timer->div_counter += cycles;
	if (timer->div_counter >= 256) {
		timer->div_counter -= 256;
		timer->div++;
	}

	if (timer->tac & 0x04) {
		int a;
		switch (timer->tac & 0x03) {
			case 0: a = 1024; break;
			case 1: a = 16; break;
			case 2: a = 64; break;
			case 3: a = 256; break;
		}
		timer->tima_counter += cycles;
		while (timer->tima_counter >= a) {
			timer->tima_counter -= a;
			timer->tima++;
			if (timer->tima == 0) {
				timer->tma = 1;
				hay_interrupt = 1;
			}
		}
	}

	return hay_interrupt;
}
