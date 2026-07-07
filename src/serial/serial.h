#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>

typedef struct Serial {
	uint8_t SB;
	uint8_t SC;
	uint8_t transfer_active;
	uint16_t bit_clock;
	uint8_t bits_left;
} Serial;

int serial_step (Serial *serial);
void serial_write_sc (Serial *serial, uint8_t val);
uint8_t serial_read_sc (Serial *serial);

#endif
