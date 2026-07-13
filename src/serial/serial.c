#include "serial/serial.h"

void serial_write_sc (Serial *serial, uint8_t val)
{
	uint8_t sc = val & 0x81;
	uint8_t was_active = serial->transfer_active;
	uint8_t old_sc = serial->SC;

	if (was_active && sc == old_sc && (sc == 0x81 || sc == 0x80))
		return;

	serial->SC = sc;

	if (sc == 0x81 && link_is_connected(serial->link))
		link_send_byte(serial->link, serial->SB);

	if (sc == 0x81 || sc == 0x80) {
		serial->transfer_active = 1;
		serial->clock = 0;
		serial->recived = 0;
		serial->shifted = 0;
	} else {
		serial->transfer_active = 0;
	}

	if (was_active && old_sc != sc) {
		uint8_t discard;
		while (link_get_byte(serial->link, &discard)) {}
		serial->recived = 0;
	}
}

static int shift (Serial *serial)
{
	uint8_t bit = (serial->buff >> (7 - serial->shifted)) & 0x01;
	serial->shifted++;
	serial->SB = (serial->SB << 1) | bit;
	if (serial->shifted > 7) return 1;
	return 0;
}

static int master_step (Serial *serial)
{
	if (!serial->recived) {
		if (!link_is_connected(serial->link)) {
			serial->buff = 0xFF;
			serial->recived = 1;
		} else {
			serial->recived = link_get_byte(serial->link, &serial->buff);
		}
	}

	if (serial->clock < 4096)
		serial->clock += 4;

	while (serial->recived && serial->shifted < 8 &&
			serial->clock >= (serial->shifted + 1) << 9) {
		(void)shift(serial);
	}

	if (serial->clock < 4096 || !serial->recived || serial->shifted < 8)
		return 0;

	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

static int slave_step (Serial *serial)
{
	if (!serial->recived) {
		if (!link_get_byte(serial->link, &serial->buff))
			return 0;
		serial->recived = 1;
		serial->clock = 0;
		serial->shifted = 0;
		link_send_byte(serial->link, serial->SB);
	}

	serial->clock += 4;
	if ((serial->clock & 511) != 0)
		return 0;

	if (!shift(serial))
		return 0;

	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

int serial_step (Serial *serial)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) return master_step(serial);
	return slave_step(serial);
}
