#include "serial/serial.h"
#include <stdio.h>

void serial_write_sc (Serial *serial, uint8_t val)
{
	serial->SC = val & 0x81;

	if ((val & 0x81) == 0x81) {
		serial->transfer_active = 1;
		serial->bit_clock = 0;
		if (serial->link)
			link_send_byte(serial->link, serial->SB);

	} else if ((val & 0x81) == 0x80) {
		serial->transfer_active = 1;
		serial->bit_clock = 0;

	} else {
		serial->transfer_active = 0;
	}
}

int serial_step (Serial *serial)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) {

		if (serial->bit_clock < 4096) {
			serial->bit_clock += 4;
			return 0;
		}

		uint8_t incoming;
		if (link_is_connected(serial->link)) {
			if (!link_get_byte(serial->link, &incoming))
				return 0;
		} else {
			incoming = 0xFF;
		}

		printf("[SC] recv=%02X\n", incoming);
		serial->SB = incoming;
		serial->transfer_active = 0;
		serial->SC &= ~0x80;
		return 1;

	} else {

		uint8_t incoming;
		if (!serial->link || !link_get_byte(serial->link, &incoming))
			return 0;

		if (serial->link)
			link_send_byte(serial->link, serial->SB);

		serial->SB = incoming;
		serial->transfer_active = 0;
		serial->SC &= ~0x80;
		return 1;
	}
}
