#include "ppu/sp_fetcher.h"
#include "gb.h"

void start_sprites (PPU *ppu) {

	if (ppu->x <= 1) {
		int upper_x = -9;
		int best_sprite = -1;
		for (int i = ppu->num_sprites - 1; i >= 0; i--) {
			if (ppu->sp_done[i]) continue;
			Sprite *sp = ppu->sprites + i;
			int start_x = sp->x - 8;
			if (start_x > 0) continue;
			if (start_x <= upper_x) continue;
			upper_x = start_x;
			best_sprite = i;
		}
		if (best_sprite != -1) {
			ppu->pending_sprite = best_sprite;
			ppu->sp_done[best_sprite] = 1;
			ppu->sprite_waiting = 1;
			return;
		}
	}

	for (int i = ppu->num_sprites - 1; i >= 0; i--) {
		if (ppu->sp_done[i]) continue;
		Sprite *sp = ppu->sprites + i;
		int start_x = sp->x - 8;

		if (ppu->x == start_x && start_x < 160) {
			ppu->pending_sprite = i;
			ppu->sp_done[i] = 1;
			ppu->sprite_waiting = 1;
			return;
		}
	}
}

static void get_sp_addr (PPU *ppu)
{
	ppu->sel_sprite = ppu->pending_sprite;

	int line = (int)ppu->ly - ((int)ppu->sprites[ppu->sel_sprite].y - 16);

	uint8_t sp_h = (ppu->lcdc & 0x04) ? 16 : 8;
	uint8_t tile_line = (ppu->sprites[ppu->sel_sprite].flags & 0x40) ?
		(sp_h - 1 - line) : line;

	uint8_t tile = ppu->sprites[ppu->sel_sprite].tile;
	if (ppu->lcdc & 0x04) tile &= 0xFE;
	ppu->sp_addr = tile << 4;
	ppu->sp_addr += tile_line << 1;
}

static void fetch_tile_low (PPU *ppu, GB *gb)
{
	uint8_t l = gb->memory.vram[ppu->sp_addr];
	for (int i = 0; i < 8; i++) {
		uint8_t bit = (ppu->sprites[ppu->sel_sprite].flags & 0x20) ?
			i : 7 - i;
		ppu->sp_buff[i].color = (l >> bit) & 1;
		ppu->sp_buff[i].pal = (ppu->sprites[ppu->sel_sprite].flags & 0x10) ?
			ppu->obp1 : ppu->obp0;
	}
}

static void fetch_tile_high (PPU *ppu, GB *gb)
{
	uint8_t h = gb->memory.vram[ppu->sp_addr + 1];
	for (int i = 0; i < 8; i++) {
		uint8_t bit = (ppu->sprites[ppu->sel_sprite].flags & 0x20) ?
			i : 7 - i;
		ppu->sp_buff[i].color |= ((h >> bit) & 1) << 1;
		ppu->sp_buff[i].bg_prio = ppu->sprites[ppu->sel_sprite].flags & 0x80;
	}
}

static void push_sprite (PPU *ppu)
{
	int start_x = (int)ppu->sprites[ppu->sel_sprite].x - 8;

	for (int i = ppu->num_sp_fifo; i < 8; i++)
		ppu->sp_fifo[i].color = 0;

	if (ppu->sprites[ppu->sel_sprite].x < 8) {
		int shift = 8 - ppu->sprites[ppu->sel_sprite].x;
		for (int i = shift; i < 8; i++) {
			ppu->sp_buff[i - shift] = ppu->sp_buff[i];
		}
		for (int i = 8 - shift; i < 8; i++) ppu->sp_buff[i].color = 0;
	}

	for (int i = 0; i < 8; i++) {
		if (!ppu->sp_buff[i].color) continue;
		if (ppu->sp_fifo[i].color != 0 && ppu->sp_fifo[i].src_x < start_x) continue;
		ppu->sp_buff[i].src_x = start_x;
		ppu->sp_fifo[i] = ppu->sp_buff[i];
	}

	ppu->sprite_active = 0;
	ppu->sprite_step = 0;
	ppu->num_sp_fifo = 8;
}

void sprite_fetch (PPU *ppu) {
	GB *gb = (GB *)ppu->bus->ctx;

	if (ppu->sprite_step == 0) {
		get_sp_addr(ppu);

	} else if (ppu->sprite_step == 2) {
		fetch_tile_low(ppu, gb);

	} else if (ppu->sprite_step == 4) {
		fetch_tile_high(ppu, gb);

	} else if (ppu->sprite_step == 5) {
		push_sprite(ppu);
		return;
	}

	ppu->sprite_step++;
}

SpritePixel get_sp_pixel (PPU *ppu) {
	SpritePixel sp = ppu->sp_fifo[0];
	for (int i = 1; i < ppu->num_sp_fifo; i++) {
		ppu->sp_fifo[i - 1] = ppu->sp_fifo[i];
	}
	ppu->num_sp_fifo--;
	return sp;
}
