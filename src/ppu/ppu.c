#include "ppu/ppu.h"
#include "gb.h"
#include <stdlib.h>

#define HBLANK 0
#define VBLANK 1
#define OAM_SCAN 2
#define DRAWING 3

static const uint32_t PALETA[4] = {
	0xFFFFFFFF,  // blanco
	0xFFAAAAAA,  // gris claro
	0xFF555555,  // gris oscuro
	0xFF000000,  // negro
};

void init_ppu_reg (PPU *ppu) {

	ppu->lcdc = 0x91;
	ppu->stat = 0x86;
	ppu->ly = 0x00;
	ppu->bgp = 0xFC;
	ppu->obp0 = 0xFF;
	ppu->obp1 = 0xFF;
	ppu->lyc = 0;

}

void init_ppu (PPU *ppu) {

	ppu->mode = OAM_SCAN;
	ppu->dots = 0;
	ppu->ready = 0;
	ppu->bus = NULL;
	ppu->x = 0;
	ppu->num_sprites = 0;
	ppu->mode3_cycles = 0;
	ppu->num_bg_fifo = 0;
	ppu->window_line = 0;
	ppu->window_active = 0;
	ppu->num_sp_fifo = 0;
	ppu->sprite_active = 0;
	ppu->sprite_step = 0;
	ppu->sprite_waiting = 0;
	ppu->pending_sprite = -1;
	ppu->sel_sprite = 0;
}

static void update_stat (PPU *ppu, int new_mode) {
	ppu->mode = new_mode;
	ppu->stat = (ppu->stat & 0xFC) | new_mode;

	int stat_irq = 0;
	if (new_mode == HBLANK) {
		ppu->mode0_cycles = 376 - ppu->mode3_cycles;
		if (ppu->mode0_cycles < 0) ppu->mode0_cycles = 0;
		if (ppu->stat & 0x08) stat_irq = 1;
	}
	if (new_mode == VBLANK && (ppu->stat & 0x10)) stat_irq = 1;
	if (new_mode == OAM_SCAN) {
		if (ppu->stat & 0x20) stat_irq = 1;
		ppu->x = 0;
		ppu->num_sprites = 0;
	}
	if (new_mode == DRAWING) {
		ppu->mode3_cycles = 0;
		ppu->x = 0;
		ppu->fetch_x = ppu->scx & ~7;

		ppu->fetcher_t = 0;
		ppu->window_active = 0;
		ppu->bg_discard = ppu->scx & 7;
		ppu->startup_tiles = 2;
		ppu->num_bg_fifo = 0;

		ppu->sprite_active = 0;
		ppu->sprite_waiting = 0;
		ppu->sprite_step = 0;
		ppu->pending_sprite = -1;
		ppu->num_sp_fifo = 0;
		for (int i = 0; i < 10; i++) {
			ppu->sp_done[i] = 0;
		}
	}

	if (stat_irq)
		ppu->bus->interrupts->IF |= 0x02;
}

static void check_lyc (PPU *ppu) {
	if (ppu->ly == ppu->lyc) {
		ppu->stat |= 0x04;
		if (ppu->stat & 0x40)
			ppu->bus->interrupts->IF |= 0x02;
	} else {
		ppu->stat &= ~0x04;
	}
}

static uint32_t decode_color (uint8_t color_id, uint8_t pal) {
	uint8_t col = (pal >> (color_id << 1)) & 0x03;
	return PALETA[col];
}

static uint8_t bg_fifo_pop (PPU *ppu) {
	uint8_t out = ppu->bg_fifo[0];
	for (int i = 1; i < ppu->num_bg_fifo; i++) {
		ppu->bg_fifo[i - 1] = ppu->bg_fifo[i];
	}
	ppu->num_bg_fifo--;
	return out;
}

static void bg_fetch (PPU *ppu) {
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

static void start_sprites (PPU *ppu) {
	for (int i = ppu->num_sprites - 1; i >= 0; i--) {
		if (ppu->sp_done[i]) continue;
		Sprite *sp = ppu->sprites + i;
		int start_x = sp->x - 8;

		if (ppu->x == start_x) {
			ppu->pending_sprite = i;
			ppu->sp_done[i] = 1;
			ppu->sprite_waiting = 1;
			return;
		}
	}
}

static void sprite_fetch (PPU *ppu) {
	GB *gb = (GB *)ppu->bus->ctx;

	if (ppu->sprite_step == 0) {

		ppu->sel_sprite = ppu->pending_sprite;

		uint8_t line = ppu->ly - (ppu->sprites[ppu->sel_sprite].y - 16);
		uint8_t sp_h = (ppu->lcdc & 0x04) ? 16 : 8;
		uint8_t tile_line = (ppu->sprites[ppu->sel_sprite].flags & 0x40) ?
			(sp_h - 1 - line) : line;

		uint8_t tile = ppu->sprites[ppu->sel_sprite].tile;
		if (ppu->lcdc & 0x04) tile &= 0xFE;
		ppu->sp_addr = tile << 4;
		ppu->sp_addr += tile_line << 1;

	} else if (ppu->sprite_step == 2) {

		uint8_t l = gb->memory.vram[ppu->sp_addr];
		for (int i = 0; i < 8; i++) {
			uint8_t bit = (ppu->sprites[ppu->sel_sprite].flags & 0x20) ?
				i : 7 - i;
			ppu->sp_buff[i].color = (l >> bit) & 1;
			ppu->sp_buff[i].pal = (ppu->sprites[ppu->sel_sprite].flags & 0x10) ?
				ppu->obp1 : ppu->obp0;
		}

	} else if (ppu->sprite_step == 4) {

		uint8_t h = gb->memory.vram[ppu->sp_addr + 1];
		for (int i = 0; i < 8; i++) {
			uint8_t bit = (ppu->sprites[ppu->sel_sprite].flags & 0x20) ?
				i : 7 - i;
			ppu->sp_buff[i].color |= ((h >> bit) & 1) << 1;
			ppu->sp_buff[i].bg_prio = ppu->sprites[ppu->sel_sprite].flags & 0x80;
		}

	} else if (ppu->sprite_step == 5) {

		for (int i = ppu->num_sp_fifo; i < 8; i++)
			ppu->sp_fifo[i].color = 0;

		for (int i = 0; i < 8; i++) {
			if (!ppu->sp_buff[i].color) continue;
			ppu->sp_fifo[i] = ppu->sp_buff[i];
		}

		ppu->sprite_active = 0;
		ppu->sprite_step = 0;
		ppu->num_sp_fifo = 8;
		return;
	}

	ppu->sprite_step++;
}

static SpritePixel get_sp_pixel (PPU *ppu) {
	SpritePixel sp = ppu->sp_fifo[0];
	for (int i = 1; i < ppu->num_sp_fifo; i++) {
		ppu->sp_fifo[i - 1] = ppu->sp_fifo[i];
	}
	ppu->num_sp_fifo--;
	return sp;
}

static uint32_t solve_priority (PPU *ppu, SpritePixel sp, uint8_t bg) {
	if (sp.color == 0) return decode_color(bg, ppu->bgp);
	if (sp.bg_prio == 0) return decode_color(sp.color, sp.pal);
	if (bg == 0) return decode_color(sp.color, sp.pal);
	return decode_color(bg, ppu->bgp);
}

static int dot_line_step (PPU *ppu) {

	if (!(ppu->lcdc & 0x01)) {
		ppu->framebuffer[ppu->ly * 160 + ppu->x] = PALETA[0];
		ppu->x++;
		ppu->mode3_cycles++;
		if (ppu->x == 160) return 1;
		return 0;
	}

	if (ppu->num_bg_fifo > 0 && ppu->startup_tiles == 0 && !ppu->sprite_active
			&& !ppu->sprite_waiting && ppu->bg_discard == 0) {

		uint8_t bg = bg_fifo_pop(ppu);
		uint32_t final_pixel;

		if (ppu->num_sp_fifo > 0) {
			SpritePixel sp = get_sp_pixel(ppu);
			final_pixel = solve_priority(ppu, sp, bg);
		} else {
			final_pixel = decode_color(bg, ppu->bgp);
		}

		ppu->framebuffer[ppu->ly * 160 + ppu->x] = final_pixel;
		ppu->x++;
	}

	if (!ppu->sprite_active) bg_fetch(ppu);

	if (ppu->startup_tiles > 0) {
		ppu->mode3_cycles++;
		return 0;
	}
	if (ppu->bg_discard > 0) {
		ppu->bg_discard--;
		(void)bg_fifo_pop(ppu);
		ppu->mode3_cycles++;
		return 0;
	}

	if (!ppu->sprite_active && !ppu->sprite_waiting && (ppu->lcdc & 0x02))
		start_sprites(ppu);

	if (ppu->sprite_waiting && ppu->num_bg_fifo > 0) {
		ppu->sprite_active = 1;
		ppu->sprite_waiting = 0;
	}

	if (ppu->sprite_active) {
		sprite_fetch(ppu);

		if (!ppu->sprite_active && !ppu->sprite_waiting && (ppu->lcdc & 0x02))
			start_sprites(ppu);

		if (ppu->sprite_waiting) {
			ppu->sprite_active = 1;
			ppu->sprite_waiting = 0;
		}

		ppu->mode3_cycles++;
		return 0;
	}

	ppu->mode3_cycles++;
	return ppu->x == 160;
}

static void scan_oam (PPU *ppu, int x) {
	if (!(ppu->lcdc & 0x02)) return;
	if (ppu->num_sprites >= 10) return;
	GB *gb = (GB *)ppu->bus->ctx;
	Sprite *sp = (Sprite *)(gb->memory.oam + (x << 2));

	uint8_t sprite_h = (ppu->lcdc & 0x04) ? 16 : 8;
	if (ppu->ly >= sp->y - 16 && ppu->ly < sp->y - 16 + sprite_h) {
		ppu->sprites[ppu->num_sprites++] = *sp;
	}
}

void ppu_step (PPU *ppu, int cycles) {
	if (!(ppu->lcdc & 0x80)) return;
	ppu->ready = 0;
	int time_dots = cycles;

	while (time_dots > 0) {
		if (ppu->mode == OAM_SCAN) {

			if (ppu->dots == 80) {
				ppu->dots = 0;
				update_stat(ppu, DRAWING);
				continue;
			}
			if (!(ppu->dots & 1)) scan_oam(ppu, ppu->x++);
		}

		else if (ppu->mode == DRAWING) {

			if (dot_line_step(ppu)) {
				if (ppu->window_active) ppu->window_line++;
				update_stat(ppu, HBLANK);
				ppu->dots = 0;
				continue;
			}

		} else if (ppu->mode == HBLANK) {

			if (ppu->dots >= ppu->mode0_cycles) {
				ppu->dots -= ppu->mode0_cycles;
				ppu->ly++;
				check_lyc(ppu);

				if (ppu->ly == 144) {
					update_stat(ppu, VBLANK);
					ppu->bus->interrupts->IF |= 0x01;
					ppu->ready = 1;
				} else {
					update_stat(ppu, OAM_SCAN);
				}
				continue;
			}

		} else {

			if (ppu->dots >= 456) {
				ppu->dots -= 456;
				ppu->ly++;

				if (ppu->ly > 153) {
					ppu->ly = 0;
					update_stat(ppu, OAM_SCAN);
					ppu->window_line = 0;
				}
				check_lyc(ppu);
				continue;
			}
		}

		time_dots--;
		ppu->dots++;
	}
}

