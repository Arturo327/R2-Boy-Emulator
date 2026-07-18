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

int serial_div_reset (Serial *serial, uint8_t old_div)
{
	if (!serial->transfer_active) return 0;
	if (!(serial->SC & 0x01)) return 0;
	if (!(old_div & 0x01)) return 0;

	if (!shift(serial)) return 0;

	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

static inline int div_bit8 (uint16_t div) {
	return (div >> 8) & 1;
}

static int step_common (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!(div_bit8(old_div) == 1 && div_bit8(div) == 0))
		return 0;

	if (!shift(serial)) return 0;

	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

static int master_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!serial->recived) {
		if (!link_is_connected(serial->link)) {
			serial->buff = 0xFF;
			serial->recived = 1;
		} else {
			serial->recived = link_get_byte(serial->link, &serial->buff);
		}
	}

	if (!serial->recived) return 0;

	return step_common(serial, old_div, div);
}

static int slave_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!serial->recived) {
		if (!link_get_byte(serial->link, &serial->buff))
			return 0;
		serial->recived = 1;
		serial->clock = 0;
		serial->shifted = 0;
		link_send_byte(serial->link, serial->SB);
	}

	return step_common(serial, old_div, div);
}

int serial_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) return master_step(serial, old_div, div);
	return slave_step(serial, old_div, div);
}
