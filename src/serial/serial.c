#include "serial/serial.h"

void serial_write_sc (Serial *serial, uint8_t val)
{
	uint8_t sc = val & 0x81;
	uint8_t was_active = serial->transfer_active;
	uint8_t old_sc = serial->SC;

	if (was_active && sc == old_sc && (sc == 0x81 || sc == 0x80))
		return;

	serial->SC = sc;

	if (sc == 0x81 || sc == 0x80) {
		serial->transfer_active = 1;
		serial->recived = 0;
		serial->shifted = 0;
		serial->sent = 0;
		serial->buff = 0xFF;
		serial->clock = 0;
	} else {
		serial->transfer_active = 0;
	}

	if (was_active && old_sc != sc) {
		uint8_t discard;
		while (link_get_byte(serial->link, &discard)) {}
		serial->recived = 0;
	}
}

static void shift (Serial *serial)
{
	uint8_t bit = (serial->buff >> (7 - serial->shifted)) & 0x01;
	serial->shifted++;
	serial->SB = (serial->SB << 1) | bit;
}

static inline int div_bit8 (uint16_t div) {
	return (div >> 8) & 1;
}

static int catchup_shift (Serial *serial)
{
	do {
		shift(serial);
	} while (serial->clock > serial->shifted && serial->shifted < 8);

	if (serial->shifted < 8) return 0;

	serial->recived = 0;
	serial->transfer_active = 0;
	serial->SC &= ~0x80;
	return 1;
}

static int master_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!serial->sent) {
		if (serial->printer) {
			printer_send_byte(serial->printer, serial->SB);
		} else if (link_is_connected(serial->link)) {
			link_send_byte(serial->link, serial->SB);
		}
		serial->sent = 1;
	}

	uint8_t div_bit = div_bit8(old_div) == 1 && div_bit8(div) == 0;
	if (div_bit) serial->clock++;

	if (!serial->recived) {
		if (serial->printer) {
			serial->recived = printer_get_byte(serial->printer, &serial->buff);
		} else if (!link_is_connected(serial->link)) {
			serial->buff = 0xFF;
			serial->recived = 1;
		} else {
			serial->recived = link_get_byte(serial->link, &serial->buff);
		}
	}

	if (!serial->recived) return 0;
	if (!div_bit) return 0;

	return catchup_shift(serial);
}

static int slave_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	uint8_t div_bit = (div_bit8(old_div) == 1 && div_bit8(div) == 0);
	if (div_bit) serial->clock++;

	if (!serial->recived) {
		if (!link_get_byte(serial->link, &serial->buff))
			return 0;

		serial->recived = 1;
		serial->shifted = 0;
		link_send_byte(serial->link, serial->SB);
	}

	if (!div_bit) return 0;

	return catchup_shift(serial);
}

int serial_div_reset (Serial *serial, uint8_t old_div)
{
	if (!serial->transfer_active) return 0;
	if (!(serial->SC & 0x01)) return 0;
	return master_step(serial, ((uint16_t)old_div) << 8, 0);
}

int serial_step (Serial *serial, uint16_t old_div, uint16_t div)
{
	if (!serial->transfer_active) return 0;

	if (serial->SC & 0x01) return master_step(serial, old_div, div);
	return slave_step(serial, old_div, div);
}
