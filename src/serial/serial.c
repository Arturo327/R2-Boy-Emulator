#include "serial/serial.h"

void serial_write_sc (Serial *serial, uint8_t val)
{
	serial->SC = val & 0x81;

	if ((val & 0x81) == 0x81) {
		serial->transfer_active = 1;
		serial->bits_left = 8;
		serial->bit_clock = 0;
	}
}

uint8_t serial_read_sc (Serial *serial)
{
	uint8_t bit7 = serial->transfer_active ? 0x80 : 0x00;
	return bit7 | (serial->SC & 0x01) | 0x7E;
}

int serial_step (Serial *serial)
{
	if (!serial->transfer_active) return 0;
	int fired = 0;

	for (int i = 0; i < 4; i++) {

		serial->bit_clock++;
		if (serial->bit_clock == 512) {
			serial->bit_clock = 0;

			serial->SB = (serial->SB << 1) | 0x01;	// TODO: replace 0x01 by incoming bit
			serial->bits_left--;

			if (serial->bits_left == 0) {
				serial->transfer_active = 0;
				serial->SC &= ~0x80;
				fired = 1;
				break;
			}
		}
	}
	return fired;
}
