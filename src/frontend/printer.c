#include "frontend/printer.h"
#include "utils/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_INIT 0x01
#define CMD_PRINT 0x02
#define CMD_FILL 0x04
#define CMD_STATUS 0x0F

#define STAT_LOW_BATTERY 0x80
#define STAT_OTHER_ERROR 0x40
#define STAT_PAPER_JAM 0x20
#define STAT_PACKET_ERROR 0x10
#define STAT_UNPROCESSED 0x08
#define STAT_IMG_FULL 0x04
#define STAT_PRINTING 0x02
#define STAT_CHECKSUM_ERR 0x01

#define PRINTER_TIMEOUT_CYCLES 419430u
#define PRINTER_PRINT_CYCLES 2097152u
#define PRINTER_MAX_FILL_LEN 0x280

static const uint8_t PRINTER_SHADES[4] = { 0xFF, 0xAA, 0x55, 0x00 };

static void printer_reset_transfer (Printer *p)
{
	p->state = PRT_MAGIC1;
	p->rle_remaining = 0;
	p->ram_len = 0;
	p->ready_to_print = 0;
	p->status = 0;
}

static uint32_t printer_pixel_color (uint8_t palette, uint8_t color_id)
{
	uint8_t shade = (palette >> (color_id * 2)) & 0x03;
	uint8_t v = PRINTER_SHADES[shade];
	return 0xFF000000u | ((uint32_t)v << 16) | ((uint32_t)v << 8) | v;
}

static uint32_t *init_print_buf (Printer *p, int width, int height)
{
	int total_pix = width * height;
	uint32_t *pixels = malloc((size_t)total_pix * sizeof(uint32_t));
	if (!pixels) {
		fprintf(stderr, "Printer: not enough memory to render the print job\n");
		return NULL;
	}

	uint32_t blank = printer_pixel_color(p->palette, 0);
	for (int i = 0; i < total_pix; i++)
		pixels[i] = blank;

	return pixels;
}

static void printer_render_row (uint32_t *row, uint8_t lo, uint8_t hi, uint8_t palette)
{
	for (int x = 0; x < 8; x++) {
		int bit = 7 - x;
		uint8_t color = ((hi >> bit & 1) << 1) | (lo >> bit & 1);
		row[x] = printer_pixel_color(palette, color);
	}
}

static void printer_render (Printer *p, uint32_t *pixels, int total_tiles, int width)
{
	uint8_t *tile = p->ram;
	uint32_t *row_base = pixels;
	int tile_col = 0;

	for (int i = 0; i < total_tiles; i++) {
		uint32_t *row = row_base + (tile_col << 3);

		for (int y = 0; y < 8; y++) {
			printer_render_row(row, tile[0], tile[1], p->palette);
			tile += 2;
			row += width;
		}

		if (++tile_col == PRINTER_TILES_PER_ROW) {
			tile_col = 0;
			row_base += width << 3;
		}
	}
}

static void save_print (Printer *p, uint32_t *pixels, int width, int height)
{
	char path[600];
	static int print_count = 0;
	if (next_numbered_file(p->path_prefix, "bmp", &print_count, path, sizeof(path))
			&& write_bmp(path, pixels, width, height))
		printf("Printer: saved print job to %s (%dx%d)\n", path, width, height);
	else
		fprintf(stderr, "Printer: could not save print job\n");
}

static void printer_render_and_save (Printer *p)
{
	int total_tiles = p->ram_len >> 4;
	if (total_tiles <= 0) return;
	int rows = (total_tiles + PRINTER_TILES_PER_ROW - 1) / PRINTER_TILES_PER_ROW;
	int width = PRINTER_TILES_PER_ROW << 3;
	int height = rows << 3;

	uint32_t *pixels = init_print_buf(p, width, height);
	if (!pixels) return;
	printer_render(p, pixels, total_tiles, width);

	save_print(p, pixels, width, height);

	free(pixels);
}

static void printer_ram_push (Printer *p, uint8_t byte)
{
	if (p->ram_len < PRINTER_RAM_SIZE) {
		p->ram[p->ram_len++] = byte;
	} else {
		p->status |= STAT_IMG_FULL;
	}
}

static void printer_feed_image_byte (Printer *p, uint8_t byte)
{
	if (!p->compression) {
		printer_ram_push(p, byte);
		return;
	}

	if (p->rle_remaining == 0) {
		if (byte & 0x80) {
			p->rle_remaining = (uint16_t)(byte & 0x7F) + 2;
			p->rle_is_repeat = 1;
		} else {
			p->rle_remaining = (uint16_t)byte + 1;
			p->rle_is_repeat = 0;
		}
		return;
	}

	if (p->rle_is_repeat) {
		for (uint16_t i = 0; i < p->rle_remaining; i++)
			printer_ram_push(p, byte);
		p->rle_remaining = 0;
	} else {
		printer_ram_push(p, byte);
		p->rle_remaining--;
	}
}

static void printer_consume_data_byte (Printer *p, uint8_t val)
{
	switch (p->command)
	{
	case CMD_PRINT:
		if (p->data_idx < 4) p->print_args[p->data_idx] = val;
		break;
	case CMD_FILL:
		printer_feed_image_byte(p, val);
		break;
	default: break;
	}
}

static void printer_fill (Printer *p)
{
	if (p->data_len > PRINTER_MAX_FILL_LEN) {
		fprintf(stderr, "Printer: CMD_FILL data length %u exceeds the %u maximum\n",
				p->data_len, PRINTER_MAX_FILL_LEN);
		p->status |= STAT_PACKET_ERROR;
	} else {
		p->status &= ~STAT_PACKET_ERROR;
	}

	if (p->data_len == 0) {
		p->ready_to_print = 1;
	} else {
		p->ready_to_print = 0;
		p->status |= STAT_UNPROCESSED;
	}
	if (p->ram_len >= PRINTER_RAM_SIZE)
		p->status |= STAT_IMG_FULL;
}

static void printer_print (Printer *p)
{
	if (p->data_len >= 4) {
		p->sheets = p->print_args[0];
		p->margins = p->print_args[1];
		p->palette = p->print_args[2];
		p->exposure = p->print_args[3];
	}

	if (!p->ready_to_print) return;
	p->ready_to_print = 0;

	if (p->sheets > 0 && p->ram_len > 0)
		printer_render_and_save(p);

	p->status |= STAT_PRINTING;
	p->status &= ~STAT_UNPROCESSED;
	p->print_cycles = PRINTER_PRINT_CYCLES;
}

static void printer_execute_command (Printer *p)
{
	if (p->checksum_calc != p->checksum_recv) {
		p->status |= STAT_CHECKSUM_ERR;
		return;
	}
	p->status &= ~STAT_CHECKSUM_ERR;

	switch (p->command)
	{
	case CMD_INIT:
		p->ram_len = 0;
		p->rle_remaining = 0;
		p->ready_to_print = 0;
		p->status = 0;
		break;

	case CMD_FILL: printer_fill(p); break;
	case CMD_PRINT: printer_print(p); break;
	case CMD_STATUS:
	default: break;
	}
}

void init_printer (Printer *p) {
	p->state = PRT_MAGIC1;
}

void attach_printer (Printer *p, const char *romfile)
{
	init_printer(p);
	if (!path_with_suffix(romfile, "_printer", p->path_prefix, sizeof(p->path_prefix))) {
		fprintf(stderr, "Printer: ROM path too long, using a generic filename prefix\n");
		snprintf(p->path_prefix, sizeof(p->path_prefix), "printer");
	}
	p->enabled = 1;
	printf("Game Boy Printer: attached (prints will be saved as %s_NNN.bmp)\n", p->path_prefix);
}

void printer_send_byte (Printer *p, uint8_t val)
{
	if (!p->enabled) return;

	if (p->state != PRT_MAGIC1 && p->idle_cycles > PRINTER_TIMEOUT_CYCLES)
		printer_reset_transfer(p);
	p->idle_cycles = 0;

	switch (p->state)
	{
	case PRT_MAGIC1:
		p->reply = 0x00;
		p->state = (val == 0x88) ? PRT_MAGIC2 : PRT_MAGIC1;
		return;

	case PRT_MAGIC2:
		p->reply = 0x00;
		if (val == 0x33) {
			p->state = PRT_COMMAND;
		} else if (val == 0x88) {
			p->state = PRT_MAGIC2;
		} else {
			p->state = PRT_MAGIC1;
		}
		return;

	case PRT_COMMAND:
		p->reply = 0x00;
		p->command = val;
		p->checksum_calc = val;
		p->state = PRT_COMPRESSION;
		return;

	case PRT_COMPRESSION:
		p->reply = 0x00;
		p->compression = val & 0x01;
		p->checksum_calc = (uint16_t)(p->checksum_calc + val);
		p->state = PRT_LEN_LOW;
		return;

	case PRT_LEN_LOW:
		p->reply = 0x00;
		p->data_len = val;
		p->checksum_calc = (uint16_t)(p->checksum_calc + val);
		p->state = PRT_LEN_HIGH;
		return;

	case PRT_LEN_HIGH:
		p->reply = 0x00;
		p->data_len = (uint16_t)(p->data_len | ((uint16_t)val << 8));
		p->checksum_calc = (uint16_t)(p->checksum_calc + val);
		p->data_idx = 0;
		p->state = (p->data_len > 0) ? PRT_DATA : PRT_CHECKSUM_LOW;
		return;

	case PRT_DATA:
		p->reply = 0x00;
		p->checksum_calc = (uint16_t)(p->checksum_calc + val);
		printer_consume_data_byte(p, val);
		p->data_idx++;
		if (p->data_idx >= p->data_len)
			p->state = PRT_CHECKSUM_LOW;
		return;

	case PRT_CHECKSUM_LOW:
		p->reply = 0x00;
		p->checksum_recv = val;
		p->state = PRT_CHECKSUM_HIGH;
		return;

	case PRT_CHECKSUM_HIGH:
		p->reply = 0x00;
		p->checksum_recv = (uint16_t)(p->checksum_recv | ((uint16_t)val << 8));
		printer_execute_command(p);
		p->state = PRT_ALIVE;
		return;

	case PRT_ALIVE:
		p->reply = 0x81;
		p->state = PRT_STATUS;
		return;

	case PRT_STATUS:
		p->reply = p->status;
		p->state = PRT_MAGIC1;
		return;
	}
}

int printer_get_byte (Printer *p, uint8_t *out)
{
	if (!p->enabled) return 0;
	*out = p->reply;
	return 1;
}

void printer_step (Printer *p)
{
	if (!p->enabled) return;

	p->idle_cycles += 4;

	if (!(p->status & STAT_PRINTING)) return;

	if (p->print_cycles > 4) {
		p->print_cycles -= 4;
	} else {
		p->print_cycles = 0;
		p->status &= ~STAT_PRINTING;
	}
}
