#include "ppu/ppu.h"
#include "gb.h"
#include <stdlib.h>
#include <stdio.h>

#define LINE0_SHORTEN 2

static const uint32_t PALETA[5] = {
	0xFFC6DE8C,	// blanco
	0xFF84A563,	// gris claro
	0xFF396139,	// gris oscuro
	0xFF081810,	// negro
	0xFFD2E6A6	// blanco "más blanco"
};

void init_ppu_reg (PPU *ppu)
{
	ppu->lcdc = 0x91;
	ppu->stat = 0x86;
	ppu->ly = 0x00;
	ppu->bgp = 0xFC;
	ppu->obp0 = 0xFF;
	ppu->obp1 = 0xFF;
	ppu->lyc = 0;

}

void init_ppu (PPU *ppu)
{
	ppu->mode = OAM_SCAN;
	ppu->bus = NULL;
	ppu->pending_sprite = -1;
}

static void update_stat_line_ex (PPU *ppu, int force_mode2)
{
	int line = 0;
	if ((ppu->stat & 0x40) && (ppu->stat & 0x04)) line = 1;
	if ((ppu->stat & 0x20) && (ppu->mode == OAM_SCAN || force_mode2)) line = 1;
	if ((ppu->stat & 0x10) && ppu->mode == VBLANK) line = 1;
	if ((ppu->stat & 0x08) && ppu->mode == HBLANK) line = 1;

	if (line && !ppu->stat_line && (ppu->lcdc & 0x80))
		ppu->bus->interrupts->IF |= 0x02;

	ppu->stat_line = line;
}

void update_stat_line (PPU *ppu) {
	update_stat_line_ex(ppu, 0);
}

void check_lyc (PPU *ppu)
{
	if (ppu->ly == ppu->lyc) {
		ppu->stat |= 0x04;
	} else {
		ppu->stat &= ~0x04;
	}
	update_stat_line(ppu);
}

static void check_lyc_delayed (PPU *ppu)
{
	if (ppu->ly == ppu->lyc) {
		ppu->lyc_delay = 2;
	} else {
		ppu->stat &= ~0x04;
		update_stat_line(ppu);
	}
}

static void update_stat (PPU *ppu, int new_mode)
{
	if (ppu->mode == new_mode) return;
	ppu->mode = new_mode;
	ppu->stat = (ppu->stat & 0xFC) | new_mode;

	if (new_mode == HBLANK) {
		ppu->mode0_cycles = 376 - ppu->mode3_cycles;
		if (ppu->mode0_cycles < 0) ppu->mode0_cycles = 0;
	}
	if (new_mode == OAM_SCAN) {
		ppu->x = 0;
		ppu->num_sprites = 0;
		ppu->oam_write_blocked = 1;
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
		ppu->sp_delay = 0;
		for (int i = 0; i < 10; i++) {
			ppu->sp_done[i] = 0;
		}

		ppu->hblank_pending = 0;
	}

	if (new_mode == VBLANK)
		update_stat_line_ex(ppu, 1);
	else
		update_stat_line(ppu);
}

static uint32_t decode_color (uint8_t color_id, uint8_t pal) {
	uint8_t col = (pal >> (color_id << 1)) & 0x03;
	return PALETA[col];
}

static uint32_t solve_priority (PPU *ppu, SpritePixel sp, uint8_t bg) {
	if (sp.color == 0) return decode_color(bg, ppu->bgp);
	if (sp.bg_prio == 0) return decode_color(sp.color, sp.pal);
	if (bg == 0) return decode_color(sp.color, sp.pal);
	return decode_color(bg, ppu->bgp);
}

static int dot_line_step (PPU *ppu) {

	if (ppu->num_bg_fifo > 0 && ppu->startup_tiles == 0 && !ppu->sprite_active
			&& !ppu->sprite_waiting && ppu->bg_discard == 0 && ppu->sp_delay == 0) {

		uint8_t bg = bg_fifo_pop(ppu);
		uint32_t final_pixel;

		if (ppu->num_sp_fifo > 0) {
			SpritePixel sp = get_sp_pixel(ppu);
			if (!(ppu->lcdc & 1)) bg = 0;
			final_pixel = solve_priority(ppu, sp, bg);
		} else {
			if (!(ppu->lcdc & 1)) final_pixel = PALETA[0];
			else final_pixel = decode_color(bg, ppu->bgp);
		}

		ppu->framebuffer[ppu->ly * 160 + ppu->x] = final_pixel;
		ppu->x++;
	}

	if (!ppu->sprite_active && ppu->sp_delay == 0) bg_fetch(ppu);

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

	if (ppu->sp_delay > 0) {
		ppu->sp_delay--;
		if (ppu->sp_delay == 0) ppu->sprite_active = 1;
		ppu->mode3_cycles++;
		return 0;
	}

	if (!ppu->sprite_active && !ppu->sprite_waiting && (ppu->lcdc & 0x02))
		start_sprites(ppu);

	if (ppu->sprite_waiting && ppu->num_bg_fifo > 0) {
		int d = 5 - (int)((ppu->x + ppu->scx) & 7);
		ppu->sp_delay = (d > 0) ? (uint8_t)d : 0;
		ppu->sprite_waiting = 0;
		if (ppu->sp_delay == 0) ppu->sprite_active = 1;
		ppu->mode3_cycles++;
		return 0;
	}

	if (ppu->sprite_active) {
		sprite_fetch(ppu);

		if (!ppu->sprite_active && !ppu->sprite_waiting && (ppu->lcdc & 0x02))
			start_sprites(ppu);

		if (ppu->sprite_waiting) {
			int d = 5 - (int)((ppu->x + ppu->scx) & 7);
			ppu->sp_delay = (d > 0) ? (uint8_t)d : 0;
			ppu->sprite_waiting = 0;
			if (ppu->sp_delay == 0) ppu->sprite_active = 1;
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

	int sprite_y = (int)sp->y - 16;
	int sprite_h = (ppu->lcdc & 0x04) ? 16 : 8;
	if ((int)ppu->ly >= sprite_y && (int)ppu->ly < sprite_y + sprite_h) {
		ppu->sprites[ppu->num_sprites++] = *sp;
	}
}

void ppu_step (PPU *ppu) {

	if (!(ppu->lcdc & 0x80)) {
		if (!ppu->lcd_was_off) {
			int pixels = sizeof(ppu->framebuffer) / sizeof(ppu->framebuffer[0]);
			for (int i = 0; i < pixels; i++) {
				ppu->framebuffer[i] = PALETA[4];
			}
		}
		ppu->ly = 0;
		ppu->window_line = 0;
		ppu->dots = 0;
		ppu->mode = HBLANK;
		ppu->stat = (ppu->stat & 0xFC) | HBLANK;
		ppu->hblank_pending = 0;
		ppu->lcd_was_off = 1;
		ppu->x = 0;
		ppu->lyc_delay = 0;
		ppu->oam_pre_block = 0;
		ppu->vram_pre_block = 0;
		return;
	}

	if (ppu->lcd_was_off) {
		ppu->lcd_was_off = 0;
		ppu->dots = 0;
		ppu->mode = HBLANK;
		ppu->stat = (ppu->stat & 0xFC) | HBLANK;
		check_lyc(ppu);
		ppu->first_line = 1;
		ppu->hblank_pending = 0;
		ppu->x = 0;
	}

	ppu->ready = 0;
	int time_dots = 4;

	if (ppu->mode == OAM_SCAN) ppu->oam_write_blocked ^= 1;

	while (time_dots > 0) {

		if (ppu->lyc_delay > 0 && --ppu->lyc_delay == 0) {
			ppu->stat |= 0x04;
			update_stat_line(ppu);
		}

		if (ppu->mode == OAM_SCAN) {

			if (ppu->dots == 76) ppu->vram_pre_block = 1;

			if (ppu->dots == 80) {
				ppu->dots = 0;
				update_stat(ppu, DRAWING);
				ppu->vram_pre_block = 0;
				continue;
			}
			if (!(ppu->dots & 1)) scan_oam(ppu, ppu->x++);

		} else if (ppu->mode == DRAWING) {

			if (ppu->hblank_pending) {
				if (ppu->window_active) ppu->window_line++;
				update_stat(ppu, HBLANK);
				ppu->hblank_pending = 0;
				ppu->dots = 0;
				continue;
			}

			if (dot_line_step(ppu)) {
				ppu->hblank_pending = 1;
			}

		} else if (ppu->mode == HBLANK) {

			if (ppu->dots == ppu->mode0_cycles - 4) {
				ppu->ly++;
				check_lyc_delayed(ppu);
				if (ppu->ly != 144) ppu->oam_pre_block = 1;
			}

			if (ppu->first_line) {

				if (ppu->dots == 0) {
					ppu->x = 0;
					ppu->num_sprites = 0;
				}
				if (ppu->dots == 77) {
					ppu->oam_pre_block = 1;
					ppu->vram_pre_block = 1;
				}
				if (ppu->dots >= 78) {
					ppu->first_line = 0;
					ppu->short_line = 1;
					ppu->dots -= 78;
					update_stat(ppu, DRAWING);
					ppu->oam_pre_block = 0;
					ppu->vram_pre_block = 0;
					continue;
				}

			} else if (ppu->short_line) {

				if (ppu->dots == ppu->mode0_cycles - LINE0_SHORTEN) {
					ppu->short_line = 0;
					ppu->dots = 0;
					update_stat(ppu, OAM_SCAN);
					ppu->oam_pre_block = 0;
					continue;
				}

			} else if (ppu->dots >= ppu->mode0_cycles) {

				ppu->dots -= ppu->mode0_cycles;

				if (ppu->ly == 144) {
					update_stat(ppu, VBLANK);
					ppu->bus->interrupts->IF |= 0x01;
					ppu->ready = 1;
				} else {
					update_stat(ppu, OAM_SCAN);
				}
				ppu->oam_pre_block = 0;
				continue;
			}

		} else {

			if (ppu->dots == 455 && ppu->ly == 153) ppu->oam_pre_block = 1;

			if (ppu->dots >= 456) {
				ppu->dots -= 456;
				ppu->ly++;

				if (ppu->ly > 153) {
					ppu->ly = 0;
					update_stat(ppu, OAM_SCAN);
					ppu->window_line = 0;
					ppu->oam_pre_block = 0;
				}
				check_lyc(ppu);
				continue;
			}
		}

		time_dots--;
		ppu->dots++;
	}
}

