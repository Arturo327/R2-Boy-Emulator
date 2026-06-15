#include "ppu/bg_fetcher.h"
#include "gb.h"

uint8_t bg_fifo_pop (PPU *ppu) {
	uint8_t out = ppu->bg_fifo[0];
	for (int i = 1; i < ppu->num_bg_fifo; i++) {
		ppu->bg_fifo[i - 1] = ppu->bg_fifo[i];
	}
	ppu->num_bg_fifo--;
	return out;
}

void bg_fetch (PPU *ppu) {
	GB *gb = (GB *)ppu->bus->ctx;

	if (!ppu->window_active && (ppu->lcdc & 0x20) && ppu->ly >= ppu->wy) {
		int win_start = (ppu->wx < 7) ? 0 : (ppu->wx - 7);
		if (ppu->x == win_start) {
			ppu->window_active = 1;
			ppu->fetcher_t = 0;
			ppu->fetch_x = 0;
			ppu->num_bg_fifo = 0;
		}
	}

	if (ppu->fetcher_t == 0) {
		uint8_t tile_x;
		uint8_t tile_y;
		uint16_t tile_map_base;

		if (ppu->window_active) {
			tile_x = ppu->fetch_x >> 3;
			tile_y = ppu->window_line >> 3;
			tile_map_base = (ppu->lcdc & 0x40) ? 0x1C00 : 0x1800;
			ppu->fetcher_bit_y = ppu->window_line & 7;
		} else {
			uint8_t bg_x = ppu->fetch_x;
			uint8_t bg_y = ppu->scy + ppu->ly;
			tile_x = bg_x >> 3;
			tile_y = bg_y >> 3;
			tile_map_base = (ppu->lcdc & 0x08) ? 0x1C00 : 0x1800;
			ppu->fetcher_bit_y = bg_y & 7;
		}

		uint16_t tile_map_addr = tile_map_base + (tile_y << 5) + tile_x;
		ppu->fetcher_tile_id = gb->memory.vram[tile_map_addr];
		ppu->fetch_x += 8;

	} else if (ppu->fetcher_t == 2) {

		uint16_t tile_addr = 0;
		if (ppu->lcdc & 0x10) tile_addr = ppu->fetcher_tile_id << 4;
		else tile_addr = 0x1000 + (((int16_t)(int8_t)ppu->fetcher_tile_id) << 4);
		uint8_t l = gb->memory.vram[tile_addr + (ppu->fetcher_bit_y << 1)];

		for (int i = 0; i < 8; i++) {
			int bit = 7 - i;
			ppu->fetcher_buffer[i] = (l >> bit) & 1;
		}

	} else if (ppu->fetcher_t == 4) {

		uint16_t tile_addr = 0;
		if (ppu->lcdc & 0x10) tile_addr = ppu->fetcher_tile_id << 4;
		else tile_addr = 0x1000 + (((int16_t)(int8_t)ppu->fetcher_tile_id) << 4);
		uint8_t h = gb->memory.vram[tile_addr + (ppu->fetcher_bit_y << 1) + 1];

		for (int i = 0; i < 8; i++) {
			int bit = 7 - i;
			ppu->fetcher_buffer[i] |= ((h >> bit) & 1) << 1;
		}

	} else if (ppu->fetcher_t >= 5) {
		if (ppu->startup_tiles > 0) {

			ppu->startup_tiles--;
			for (int i = 0; i < 8; i++) {
				ppu->bg_fifo[ppu->num_bg_fifo++] = ppu->fetcher_buffer[i];
			}
			ppu->fetcher_t = 0;
		}
		if (ppu->num_bg_fifo == 0) {

			for (int i = 0; i < 8; i++) {
				ppu->bg_fifo[ppu->num_bg_fifo++] = ppu->fetcher_buffer[i];
			}
			ppu->fetcher_t = 0;
		}
		return;
	}

	ppu->fetcher_t++;
}
