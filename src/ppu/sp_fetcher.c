#include "ppu/sp_fetcher.h"
#include "gb.h"

#define PRIORITY 0x80
#define Y_FLIP 0x40
#define X_FLIP 0x20
#define PALETTE 0x10

void start_sprites (PPU *ppu) {

	if (ppu->x <= 1) {
		int upper_x = -9;
		int best_sprite = -1;
		for (int i = ppu->sp.num_sprites - 1; i >= 0; i--) {
			if (ppu->sp.done[i]) continue;
			Sprite *sp = ppu->sp.sprites + i;
			int start_x = sp->x - 8;
			if (start_x > 0) continue;
			if (start_x <= upper_x) continue;
			upper_x = start_x;
			best_sprite = i;
		}
		if (best_sprite != -1) {
			ppu->sp.pending = best_sprite;
			ppu->sp.done[best_sprite] = 1;
			ppu->sp.sprite_waiting = 1;
			return;
		}
	}

	for (int i = ppu->sp.num_sprites - 1; i >= 0; i--) {
		if (ppu->sp.done[i]) continue;
		Sprite *sp = ppu->sp.sprites + i;
		int start_x = sp->x - 8;

		if (ppu->x == start_x && start_x < 160) {
			ppu->sp.pending = i;
			ppu->sp.done[i] = 1;
			ppu->sp.sprite_waiting = 1;
			return;
		}
	}
}

static void get_sp_addr (PPU *ppu)
{
	ppu->sp.sel_sprite = ppu->sp.pending;

	int line = (int)ppu->ly - ((int)ppu->sp.sprites[ppu->sp.sel_sprite].y - 16);

	uint8_t sp_h = (ppu->lcdc & SP_SIZE) ? 16 : 8;
	uint8_t tile_line = (ppu->sp.sprites[ppu->sp.sel_sprite].flags & Y_FLIP) ?
		(sp_h - 1 - line) : line;

	uint8_t tile = ppu->sp.sprites[ppu->sp.sel_sprite].tile;
	if (ppu->lcdc & SP_SIZE) tile &= 0xFE;
	ppu->sp.addr = tile << 4;
	ppu->sp.addr += tile_line << 1;
}

static void fetch_tile_low (PPU *ppu, GB *gb)
{
	uint8_t l = gb->memory.vram[ppu->sp.addr];
	for (int i = 0; i < 8; i++) {
		uint8_t bit = (ppu->sp.sprites[ppu->sp.sel_sprite].flags & X_FLIP) ?
			i : 7 - i;
		ppu->sp.buff[i].color = (l >> bit) & 1;
		ppu->sp.buff[i].pal = (ppu->sp.sprites[ppu->sp.sel_sprite].flags & PALETTE) ?
			ppu->obp1 : ppu->obp0;
	}
}

static void fetch_tile_high (PPU *ppu, GB *gb)
{
	uint8_t h = gb->memory.vram[ppu->sp.addr + 1];
	for (int i = 0; i < 8; i++) {
		uint8_t bit = (ppu->sp.sprites[ppu->sp.sel_sprite].flags & X_FLIP) ?
			i : 7 - i;
		ppu->sp.buff[i].color |= ((h >> bit) & 1) << 1;
		ppu->sp.buff[i].bg_prio = ppu->sp.sprites[ppu->sp.sel_sprite].flags & PRIORITY;
	}
}

static void push_sprite (PPU *ppu)
{
	int start_x = (int)ppu->sp.sprites[ppu->sp.sel_sprite].x - 8;

	for (int i = ppu->sp.num_fifo; i < 8; i++)
		ppu->sp.fifo[i].color = 0;

	if (ppu->sp.sprites[ppu->sp.sel_sprite].x < 8) {
		int shift = 8 - ppu->sp.sprites[ppu->sp.sel_sprite].x;
		for (int i = shift; i < 8; i++) {
			ppu->sp.buff[i - shift] = ppu->sp.buff[i];
		}
		for (int i = 8 - shift; i < 8; i++) ppu->sp.buff[i].color = 0;
	}

	for (int i = 0; i < 8; i++) {
		if (!ppu->sp.buff[i].color) continue;
		if (ppu->sp.fifo[i].color != 0 && ppu->sp.fifo[i].src_x < start_x) continue;
		ppu->sp.buff[i].src_x = start_x;
		ppu->sp.fifo[i] = ppu->sp.buff[i];
	}

	ppu->sp.sprite_active = 0;
	ppu->sp.step = 0;
	ppu->sp.num_fifo = 8;
}

void sprite_fetch (PPU *ppu) {
	GB *gb = (GB *)ppu->bus->ctx;

	if (ppu->sp.step == 0) {
		get_sp_addr(ppu);

	} else if (ppu->sp.step == 2) {
		fetch_tile_low(ppu, gb);

	} else if (ppu->sp.step == 4) {
		fetch_tile_high(ppu, gb);

	} else if (ppu->sp.step == 5) {
		push_sprite(ppu);
		return;
	}

	ppu->sp.step++;
}

SpritePixel get_sp_pixel (PPU *ppu) {
	SpritePixel sp = ppu->sp.fifo[0];
	for (int i = 1; i < ppu->sp.num_fifo; i++) {
		ppu->sp.fifo[i - 1] = ppu->sp.fifo[i];
	}
	ppu->sp.num_fifo--;
	return sp;
}
