#include "serial/serial.h"

void serial_write_sc (Serial *serial, uint8_t val)
{
	serial->SC = val & 0x81;

	if ((val & 0x81) == 0x81) {
		serial->transfer_active = 1;
		serial->clock = 0;
		if (link_is_connected(serial->link))
			link_send_byte(serial->link, serial->SB);

	} else if ((val & 0x81) == 0x80) {
		serial->transfer_active = 1;
		serial->clock = 0;

	} else {
		serial->transfer_active = 0;
	}
}

int serial_step (Serial *serial)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) {

		if (serial->clock < 4096) {
			serial->clock += 4;
			return 0;
		}

		uint8_t incoming;
		if (link_is_connected(serial->link)) {
			if (!link_get_byte(serial->link, &incoming))
				return 0;
		} else {
			incoming = 0xFF;
		}

		serial->SB = incoming;
		serial->transfer_active = 0;
		serial->SC &= ~0x80;
		return 1;

	} else {

		if (!serial->recived) {
			if (!link_get_byte(serial->link, &serial->buff))
				return 0;
			serial->recived = 1;
			serial->clock = 0;
		}

		if (serial->clock < 4096) {
			serial->clock += 4;
			return 0;
		}

		link_send_byte(serial->link, serial->SB);

		serial->SB = serial->buff;
		serial->recived = 0;
		serial->transfer_active = 0;
		serial->SC &= ~0x80;
		return 1;
	}
}
