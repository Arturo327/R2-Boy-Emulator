#include "ppu/ppu.h"
#include "gb.h"
#include <stdlib.h>
#include <stdio.h>

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
	ppu->sp.pending = -1;
	ppu->palette = PAL_DEFAULT;
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

static void reset_drawing (PPU *ppu)
{
	ppu->mode3_cycles = 0;
	ppu->x = 0;
	ppu->fetch_x = ppu->scx & ~7;

	ppu->bg.fetcher_t = 0;
	ppu->bg.window_active = 0;

	int a = ppu->scx & 7;
	ppu->bg.discard_px = a;
	if (a == 0) ppu->bg.discard = 0;
	else if (a <= 4) ppu->bg.discard = 4;
	else ppu->bg.discard = 8;

	ppu->bg.startup_tiles = 2;
	ppu->bg.num_fifo = 0;

	ppu->sp.sprite_active = 0;
	ppu->sp.sprite_waiting = 0;
	ppu->sp.step = 0;
	ppu->sp.pending = -1;
	ppu->sp.num_fifo = 0;
	ppu->sp.tiles_touched = 0;
	ppu->sp.delay = 0;
	for (int i = 0; i < 10; i++) {
		ppu->sp.done[i] = 0;
	}

	ppu->hblank_pending = 0;
}

static void update_stat (PPU *ppu, PPU_Mode new_mode)
{
	if (ppu->mode == new_mode) return;
	ppu->mode = new_mode;
	ppu->stat = (ppu->stat & 0xFC) | new_mode;

	if (new_mode == HBLANK) {
		ppu->mode0_cycles = DRAWING_HBLANK_DOTS - ppu->mode3_cycles;
	}
	if (new_mode == OAM_SCAN) {
		ppu->x = 0;
		ppu->sp.num_sprites = 0;
		ppu->oam_write_blocked = 1;
	}
	if (new_mode == DRAWING) {
		reset_drawing(ppu);
	}

	if (new_mode == VBLANK)
		update_stat_line_ex(ppu, 1);
	else
		update_stat_line(ppu);
}

static uint32_t decode_color (PPU *ppu, uint8_t color_id, uint8_t pal) {
	uint8_t col = (pal >> (color_id << 1)) & 0x03;
	return PALETTES[ppu->palette][col];
}

static uint32_t solve_priority (PPU *ppu, SpritePixel sp, uint8_t bg) {
	if (sp.color == 0) return decode_color(ppu, bg, ppu->bgp);
	if (sp.bg_prio == 0) return decode_color(ppu, sp.color, sp.pal);
	if (bg == 0) return decode_color(ppu, sp.color, sp.pal);
	return decode_color(ppu, bg, ppu->bgp);
}

static void calc_sp_delay (PPU *ppu)
{
	int sprite_x = ppu->sp.sprites[ppu->sp.pending].x;
	int col = (sprite_x + ppu->scx) & 7;
	int tile = ((sprite_x + ppu->scx) & 0xFF) >> 3;

	if (ppu->sp.tiles_touched & (1 << tile)) {
		ppu->sp.delay = 0;
	} else {
		ppu->sp.tiles_touched |= (1 << tile);
		int d = 5 - col;
		ppu->sp.delay = (d > 0) ? (uint8_t)d : 0;
	}
}

static void draw_pixel (PPU *ppu)
{
	if (ppu->bg.num_fifo == 0 || ppu->bg.startup_tiles > 0) return;
	if (ppu->sp.sprite_active || ppu->sp.sprite_waiting) return;
	if (ppu->bg.discard > 0 || ppu->sp.delay > 0) return;

	uint8_t bg = bg_fifo_pop(ppu);
	uint32_t final_pixel;

	if (ppu->sp.num_fifo > 0) {
		SpritePixel sp = get_sp_pixel(ppu);
		if (!(ppu->lcdc & 1)) bg = 0;
		final_pixel = solve_priority(ppu, sp, bg);
	} else {
		if (!(ppu->lcdc & 1)) final_pixel = PALETTES[ppu->palette][0];
		else final_pixel = decode_color(ppu, bg, ppu->bgp);
	}

	ppu->framebuffer[ppu->ly * 160 + ppu->x] = final_pixel;
	ppu->x++;	
}

static inline void begin_sprite_delay (PPU *ppu)
{
	calc_sp_delay(ppu);
	ppu->sp.sprite_waiting = 0;
	if (ppu->sp.delay == 0) ppu->sp.sprite_active = 1;
}

static int handle_sprites (PPU *ppu)
{
	if (!ppu->sp.sprite_active && !ppu->sp.sprite_waiting && (ppu->lcdc & 0x02))
		start_sprites(ppu);

	if (ppu->sp.sprite_waiting && ppu->bg.num_fifo > 0) {
		begin_sprite_delay(ppu);
		return 1;
	}

	if (ppu->sp.sprite_active) {
		sprite_fetch(ppu);

		if (!ppu->sp.sprite_active && !ppu->sp.sprite_waiting && (ppu->lcdc & 0x02))
			start_sprites(ppu);

		if (ppu->sp.sprite_waiting) {
			begin_sprite_delay(ppu);
		}
		return 1;
	}

	return 0;
}

static int dot_line_step (PPU *ppu)
{
	if (ppu->sp.delay > 0) {
		ppu->sp.delay--;
		if (ppu->sp.delay == 0) ppu->sp.sprite_active = 1;
		ppu->mode3_cycles++;
		return 0;
	}

	draw_pixel(ppu);

	uint8_t prev_startup = ppu->bg.startup_tiles;
	if (!ppu->sp.sprite_active && ppu->sp.delay == 0) bg_fetch(ppu);

	if (prev_startup > 0) {
		ppu->mode3_cycles++;
		return 0;
	}
	if (ppu->bg.discard > 0) {
		ppu->bg.discard--;
		if (ppu->bg.discard_px > 0 && ppu->bg.num_fifo > 0) {
			ppu->bg.discard_px--;
			(void)bg_fifo_pop(ppu);
		}
		ppu->mode3_cycles++;
		return 0;
	}

	if (handle_sprites(ppu)) {
		ppu->mode3_cycles++;
		return 0;
	}

	ppu->mode3_cycles++;
	return ppu->x == 160;
}

static void scan_oam (PPU *ppu, int x)
{
	if (!(ppu->lcdc & 0x02)) return;
	if (ppu->sp.num_sprites >= 10) return;
	GB *gb = (GB *)ppu->bus->ctx;
	Sprite *sp = (Sprite *)(gb->memory.oam + (x << 2));

	int sprite_y = (int)sp->y - 16;
	int sprite_h = (ppu->lcdc & 0x04) ? 16 : 8;
	if ((int)ppu->ly >= sprite_y && (int)ppu->ly < sprite_y + sprite_h) {
		ppu->sp.sprites[ppu->sp.num_sprites++] = *sp;
	}
}

static void turn_lcd_off (PPU *ppu)
{
	if (!ppu->lcd_was_off) {
		int pixels = sizeof(ppu->framebuffer) / sizeof(ppu->framebuffer[0]);
		for (int i = 0; i < pixels; i++) {
			ppu->framebuffer[i] = PALETTES[ppu->palette][4];
		}
	}
	ppu->ly = 0;
	ppu->bg.window_line = 0;
	ppu->dots = 0;
	ppu->mode = HBLANK;
	ppu->stat = (ppu->stat & 0xFC) | HBLANK;
	ppu->hblank_pending = 0;
	ppu->lcd_was_off = 1;
	ppu->x = 0;
	ppu->lyc_delay = 0;
	ppu->oam_pre_block = 0;
	ppu->vram_pre_block = 0;
}

static void turn_lcd_on (PPU *ppu)
{
	ppu->lcd_was_off = 0;
	ppu->dots = 0;
	ppu->mode = HBLANK;
	ppu->stat = (ppu->stat & 0xFC) | HBLANK;
	check_lyc(ppu);
	ppu->first_line = 1;
	ppu->hblank_pending = 0;
	ppu->x = 0;
}

static void finish_hblank (PPU *ppu)
{
	ppu->dots -= ppu->mode0_cycles;

	if (ppu->ly == DRAWING_LINES) {
		update_stat(ppu, VBLANK);
		ppu->bus->interrupts->IF |= 0x01;
		ppu->ready = 1;
	} else {
		update_stat(ppu, OAM_SCAN);
	}
	ppu->oam_pre_block = 0;
}

static int handle_hblank_first_line (PPU *ppu)
{
	if (ppu->dots == 0) {
		ppu->x = 0;
		ppu->sp.num_sprites = 0;

	} else if (ppu->dots == OAM_SCAN_DOTS - 3) {
		ppu->oam_pre_block = 1;
		ppu->vram_pre_block = 1;

	} else if (ppu->dots >= OAM_SCAN_DOTS - 2) {
		ppu->first_line = 0;
		ppu->short_line = 1;
		ppu->dots -= OAM_SCAN_DOTS - 2;
		update_stat(ppu, DRAWING);
		ppu->oam_pre_block = 0;
		ppu->vram_pre_block = 0;
		return 1;
	}
	return 0;
}

static int handle_hblank (PPU *ppu)
{
	if (ppu->dots == ppu->mode0_cycles - 4) {
		ppu->ly++;
		check_lyc_delayed(ppu);
		if (ppu->ly != DRAWING_LINES) ppu->oam_pre_block = 1;
	}

	if (ppu->first_line) {
		if (handle_hblank_first_line(ppu)) return 1;

	} else if (ppu->short_line) {
		if (ppu->dots == ppu->mode0_cycles - 2) {
			ppu->short_line = 0;
			ppu->dots = 0;
			update_stat(ppu, OAM_SCAN);
			ppu->oam_pre_block = 0;
			return 1;
		}

	} else if (ppu->dots >= ppu->mode0_cycles) {
		finish_hblank(ppu);
		return 1;
	}
	return 0;
}

static int handle_vblank (PPU *ppu)
{
	if (ppu->dots == LINE_DOTS - 1 && ppu->ly == TOTAL_LINES - 1)
		ppu->oam_pre_block = 1;

	if (ppu->dots >= LINE_DOTS) {
		ppu->dots -= LINE_DOTS;
		ppu->ly++;

		if (ppu->ly >= TOTAL_LINES) {
			ppu->ly = 0;
			update_stat(ppu, OAM_SCAN);
			ppu->bg.window_line = 0;
			ppu->oam_pre_block = 0;
		}
		check_lyc(ppu);
		return 1;
	}
	return 0;
}

static int handle_drawing (PPU *ppu)
{
	if (ppu->hblank_pending) {
		if (ppu->bg.window_active) ppu->bg.window_line++;
		update_stat(ppu, HBLANK);
		ppu->hblank_pending = 0;
		ppu->dots = 0;
		return 1;
	}

	if (dot_line_step(ppu))
		ppu->hblank_pending = 1;

	return 0;
}

static int handle_oam_scan (PPU *ppu)
{
	if (ppu->dots == OAM_SCAN_DOTS - 4) ppu->vram_pre_block = 1;

	if (ppu->dots == OAM_SCAN_DOTS) {
		ppu->dots = 0;
		update_stat(ppu, DRAWING);
		ppu->vram_pre_block = 0;
		return 1;
	}
	if (!(ppu->dots & 1)) scan_oam(ppu, ppu->x++);

	return 0;
}

void ppu_step (PPU *ppu)
{
	if (!(ppu->lcdc & 0x80)) {
		turn_lcd_off(ppu);
		return;
	}

	if (ppu->lcd_was_off) turn_lcd_on(ppu);

	ppu->ready = 0;
	int time_dots = 4;

	if (ppu->mode == OAM_SCAN) ppu->oam_write_blocked ^= 1;

	while (time_dots > 0) {

		if (ppu->lyc_delay > 0 && --ppu->lyc_delay == 0) {
			ppu->stat |= 0x04;
			update_stat_line(ppu);
		}

		if (ppu->mode == OAM_SCAN) {
			if (handle_oam_scan(ppu)) continue;

		} else if (ppu->mode == DRAWING) {
			if (handle_drawing(ppu)) continue;

		} else if (ppu->mode == HBLANK) {
			if (handle_hblank(ppu)) continue;

		} else if (ppu->mode == VBLANK) {
			if (handle_vblank(ppu)) continue;
		}

		time_dots--;
		ppu->dots++;
	}
}

