#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "frontend/config.h"

#define OAM_SCAN_DOTS 80
#define LINE_DOTS 456
#define DRAWING_HBLANK_DOTS 376
#define DRAWING_LINES 144
#define TOTAL_LINES 154

#define PPU_ENABLE 0x80
#define WIN_TILE_MAP 0x40
#define WIN_ENABLE 0x20
#define BG_WIN_TILES 0x10
#define BG_TILE_MAP 0x08
#define SP_SIZE 0x04
#define SP_ENABLE 0x02
#define BG_WIN_PRIO 0x01

typedef enum {
	HBLANK,
	VBLANK,
	OAM_SCAN,
	DRAWING
} PPU_Mode;

typedef enum {
	STOP_FREEZE_NONE = 0,
	STOP_FREEZE_BLACK,
	STOP_FREEZE_BG_ONLY
} StopFreeze;

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

typedef struct BgPixel {
	uint8_t color;
	uint8_t attr;
} BgPixel;

typedef struct SpriteFetcher {
	Sprite sprites[10];
	uint8_t num_sprites;

	uint8_t sel_sprite;
	uint8_t sprite_active;
	uint8_t sprite_waiting;
	uint8_t step;
	uint8_t delay;
	uint32_t tiles_touched;

	int8_t pending;
	uint8_t done[10];

	SpritePixel fifo[8];
	uint8_t num_fifo;
	uint8_t fifo_head;
	SpritePixel buff[8];
	uint16_t addr;
} SpriteFetcher;

typedef struct BgFetcher {
	BgPixel fifo[16];
	uint8_t num_fifo;
	uint8_t fifo_head;
	uint8_t buffer[8];
	uint8_t buffer_attr;

	uint8_t discard;
	uint8_t discard_px;
	uint8_t startup_tiles;

	uint8_t window_active;
	uint8_t window_line;

	uint8_t fetcher_t;
	uint8_t fetcher_tile_id;
	uint8_t fetcher_bit_y;
	uint8_t fetcher_attr;
} BgFetcher;

typedef struct PPU {
	uint32_t framebuffer[160 * 144];

	SpriteFetcher sp;
	BgFetcher bg;

	uint8_t hblank_pending;
	uint8_t lyc_delay;
	uint8_t oam_pre_block;
	uint8_t vram_pre_block;
	uint8_t oam_write_blocked;

	PPU_Mode mode;
	uint16_t dots;
	uint8_t ready;
	uint8_t lcd_was_off;
	uint8_t stop_glitch_pending;
	StopFreeze stop_freeze;

	uint8_t shutdown_pending;
	uint8_t shutdown_frame;

	uint8_t x;
	uint16_t line_base;
	uint8_t fetch_x;
	uint8_t short_line;
	uint8_t first_line;

	uint16_t mode3_cycles;
	uint16_t mode0_cycles;

	Bus *bus;
	DmgPalette palette;
	Model model;

	uint8_t stat_line;
	uint8_t lcdc, stat, scy, scx, ly, lyc, dma, wy, wx;
	uint8_t bgp, obp0, obp1;	// DMG only
	uint8_t opri, bcps, ocps;	// CGB only
} PPU;

#endif
