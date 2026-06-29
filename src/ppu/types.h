#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define HBLANK 0
#define VBLANK 1
#define OAM_SCAN 2
#define DRAWING 3

typedef struct Sprite {
	uint8_t y;
	uint8_t x;
	uint8_t tile;
	uint8_t flags;
} Sprite;

typedef struct SpritePixel {
	uint8_t color;
	uint8_t pal;
	uint8_t bg_prio;
	int16_t src_x;
} SpritePixel;

typedef struct PPU {
	uint32_t framebuffer[160 * 144];

	Sprite sprites[10];
	int num_sprites;

	uint8_t sel_sprite;
	uint8_t sprite_active;
	uint8_t sprite_waiting;
	uint8_t sprite_step;
	uint8_t sp_delay;

	int pending_sprite;
	uint8_t sp_done[10];

	SpritePixel sp_fifo[8];
	uint8_t num_sp_fifo;
	SpritePixel sp_buff[8];
	uint16_t sp_addr;

	uint8_t bg_fifo[16];
	uint8_t num_bg_fifo;
	uint8_t fetcher_buffer[8];
	uint8_t bg_discard;
	uint8_t startup_tiles;

	uint8_t hblank_pending;
	int mode;
	int dots;
	int ready;
	uint8_t lcd_was_off;

	int x;
	uint8_t fetch_x;
	uint8_t oam_startup;
	uint8_t short_line;

	uint8_t fetcher_t;
	uint8_t fetcher_tile_id;
	uint8_t fetcher_bit_y;

	uint8_t window_active;
	uint8_t window_line;

	int mode3_cycles;
	int mode0_cycles;

	Bus *bus;

	uint8_t stat_line;
	uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wy, wx;
} PPU;

#endif
