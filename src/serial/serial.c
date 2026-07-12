#include "serial/serial.h"

void serial_write_sc (Serial *serial, uint8_t val)
{
	uint8_t sc = val & 0x81;
	uint8_t was_active = serial->transfer_active;
	uint8_t old_sc = serial->SC;

	if (was_active && sc == old_sc && (sc == 0x81 || sc == 0x80))
		return;

	serial->SC = sc;

	if (sc == 0x81) {
		serial->transfer_active = 1;
		serial->clock = 0;
		if (link_is_connected(serial->link))
			link_send_byte(serial->link, serial->SB);

	} else if (sc == 0x80) {
		serial->transfer_active = 1;
		serial->clock = 0;
		serial->recived = 0;

	} else {
		serial->transfer_active = 0;
	}

	if (was_active && old_sc != sc) {
		uint8_t discard;
		while (link_get_byte(serial->link, &discard)) {}
		serial->recived = 0;
	}
}

static int master (Serial *serial)
{
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
}

static int slave (Serial *serial)
{
	if (!serial->recived) {
		if (!link_get_byte(serial->link, &serial->buff))
			return 0;
		serial->recived = 1;
		serial->clock = 0;
		link_send_byte(serial->link, serial->SB);
	}

	if (serial->clock < 4096) {
		serial->clock += 4;
		return 0;
	}

	serial->SB = serial->buff;
	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

int serial_step (Serial *serial)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) return master(serial);
	else return slave(serial);
}
