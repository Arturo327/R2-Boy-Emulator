#ifndef PRINTER_H
#define PRINTER_H

#include <stdint.h>

#define PRINTER_RAM_SIZE 8192
#define PRINTER_TILES_PER_ROW 20

typedef enum {
	PRT_MAGIC1 = 0,
	PRT_MAGIC2,
	PRT_COMMAND,
	PRT_COMPRESSION,
	PRT_LEN_LOW,
	PRT_LEN_HIGH,
	PRT_DATA,
	PRT_CHECKSUM_LOW,
	PRT_CHECKSUM_HIGH,
	PRT_ALIVE,
	PRT_STATUS
} PrinterState;

typedef struct Printer {
	uint8_t enabled;

	PrinterState state;
	uint32_t idle_cycles;

	uint8_t command;
	uint8_t compression;
	uint16_t data_len;
	uint16_t data_idx;
	uint16_t checksum_calc;
	uint16_t checksum_recv;

	uint16_t rle_remaining;
	uint8_t rle_is_repeat;

	uint8_t print_args[4];
	uint8_t sheets;
	uint8_t margins;
	uint8_t palette;
	uint8_t exposure;
	uint8_t ready_to_print;

	uint8_t ram[PRINTER_RAM_SIZE];
	uint16_t ram_len;

	uint8_t status;
	uint32_t print_cycles;
	uint8_t reply;

	char path_prefix[512];
} Printer;

void init_printer (Printer *p);
void attach_printer (Printer *p, const char *romfile);

void printer_send_byte (Printer *p, uint8_t val);
int printer_get_byte (Printer *p, uint8_t *out);

void printer_step (Printer *p);

#endif
