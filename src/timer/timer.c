#include "timer/timer.h"

static int get_tac_bit (Timer *timer)
{
	switch (timer->tac & 0x03) {
		case 0: return (timer->div >> 9) & 1;
		case 1: return (timer->div >> 3) & 1;
		case 2: return (timer->div >> 5) & 1;
		case 3: return (timer->div >> 7) & 1;
	}
	return 0;
}

int timer_step (Timer *timer)
{
	int hay_interrupt = 0;
	timer->reload = 0;

	for (int i = 0; i < 4; i++) {

		int bit = get_tac_bit(timer);
		int tima_enable = (timer->tac & 0x04) ? 1 : 0;
		int before = bit & tima_enable;

		timer->div++;

		bit = get_tac_bit(timer);
		int after = bit & tima_enable;

		if (timer->tima_overflow > 0) {
			timer->tima_overflow--;
			if (timer->tima_overflow == 0) {
				timer->tima = timer->tma;
				timer->reload = 1;
				hay_interrupt = 1;
			}
		}

		if (before == 1 && after == 0) {
			timer->tima++;
			if (timer->tima == 0 && !timer->reload) {
				timer->tima_overflow = 4;
			}
		}
	}

	return hay_interrupt;
}
