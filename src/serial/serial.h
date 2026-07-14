#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include "frontend/link.h"

typedef struct Serial {
	uint8_t SB;
	uint8_t SC;

	uint8_t transfer_active;
	uint16_t clock;

	uint8_t shifted;
	uint8_t recived;
	uint8_t buff;

	Link *link;
} Serial;

int serial_step (Serial *serial, uint16_t old_div, uint16_t div);
void serial_write_sc (Serial *serial, uint8_t val);

#endif
