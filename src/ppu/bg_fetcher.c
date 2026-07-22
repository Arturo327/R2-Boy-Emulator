#include "ppu/bg_fetcher.h"
#include "gb.h"

uint8_t bg_fifo_pop (PPU *ppu) {
	uint8_t out = ppu->bg.fifo[ppu->bg.fifo_head];
	ppu->bg.fifo_head = (ppu->bg.fifo_head + 1) & 15;
	ppu->bg.num_fifo--;
	return out;
}

static void start_window (PPU *ppu)
{
	if (ppu->bg.window_active || !(ppu->lcdc & WIN_ENABLE) || ppu->ly < ppu->wy) return;

	int win_start = (ppu->wx < 7) ? 0 : (ppu->wx - 7);
	if (ppu->x != win_start) return;

	ppu->bg.window_active = 1;
	ppu->bg.fetcher_t = 0;
	ppu->fetch_x = 0;
	ppu->bg.num_fifo = 0;
	ppu->bg.discard = 0;
	ppu->bg.discard_px = 0;
	ppu->bg.startup_tiles = 0;
}

static void fetch_tile_id (GB *gb, PPU *ppu)
{
	uint8_t tile_x;
	uint8_t tile_y;
	uint16_t tile_map_base;

	if (ppu->bg.window_active) {
		tile_x = ppu->fetch_x >> 3;
		tile_y = ppu->bg.window_line >> 3;
		tile_map_base = (ppu->lcdc & WIN_TILE_MAP) ? 0x1C00 : 0x1800;
		ppu->bg.fetcher_bit_y = ppu->bg.window_line & 7;
	} else {
		uint8_t bg_x = ppu->fetch_x;
		uint8_t bg_y = ppu->scy + ppu->ly;
		tile_x = bg_x >> 3;
		tile_y = bg_y >> 3;
		tile_map_base = (ppu->lcdc & BG_TILE_MAP) ? 0x1C00 : 0x1800;
		ppu->bg.fetcher_bit_y = bg_y & 7;
	}

	uint16_t tile_map_addr = tile_map_base + (tile_y << 5) + tile_x;
	ppu->bg.fetcher_tile_id = gb->memory.vram[tile_map_addr];
	ppu->fetch_x += 8;
}

static void fetch_tile_low (GB *gb, PPU *ppu)
{
	uint16_t tile_addr = 0;
	if (ppu->lcdc & BG_WIN_TILES) tile_addr = ppu->bg.fetcher_tile_id << 4;
	else tile_addr = 0x1000 + (((int16_t)(int8_t)ppu->bg.fetcher_tile_id) << 4);
	uint8_t l = gb->memory.vram[tile_addr + (ppu->bg.fetcher_bit_y << 1)];

	for (int i = 0; i < 8; i++) {
		int bit = 7 - i;
		ppu->bg.buffer[i] = (l >> bit) & 1;
	}
}

static void fetch_tile_high (GB *gb, PPU *ppu)
{
	uint16_t tile_addr = 0;
	if (ppu->lcdc & BG_WIN_TILES) tile_addr = ppu->bg.fetcher_tile_id << 4;
	else tile_addr = 0x1000 + (((int16_t)(int8_t)ppu->bg.fetcher_tile_id) << 4);
	uint8_t h = gb->memory.vram[tile_addr + (ppu->bg.fetcher_bit_y << 1) + 1];

	for (int i = 0; i < 8; i++) {
		int bit = 7 - i;
		ppu->bg.buffer[i] |= ((h >> bit) & 1) << 1;
	}
}

static void push (PPU *ppu)
{
	for (int i = 0; i < 8; i++) {
		ppu->bg.fifo[(ppu->bg.fifo_head + ppu->bg.num_fifo) & 15] = ppu->bg.buffer[i];
		ppu->bg.num_fifo++;
	}
	ppu->bg.fetcher_t = 0;
}

static void push_tile (PPU *ppu)
{
	if (ppu->bg.startup_tiles > 0) {
		ppu->bg.startup_tiles--;
		push(ppu);
	}
	if (ppu->bg.num_fifo == 0) {
		push(ppu);
	}
}

void bg_fetch (PPU *ppu)
{
	GB *gb = (GB *)ppu->bus->ctx;

	start_window(ppu);

	if (ppu->bg.fetcher_t == 0) {
		fetch_tile_id(gb, ppu);

	} else if (ppu->bg.fetcher_t == 2) {
		fetch_tile_low(gb, ppu);

	} else if (ppu->bg.fetcher_t == 4) {
		fetch_tile_high(gb, ppu);

	} else if (ppu->bg.fetcher_t >= 5) {
		push_tile(ppu);
		return;
	}

	ppu->bg.fetcher_t++;
}
