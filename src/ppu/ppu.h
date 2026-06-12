#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <bus/bus.h>

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
} SpritePixel;

typedef struct PPU {
	uint32_t framebuffer[160 * 144];

	Sprite sprites[10];
	int num_sprites;

	uint8_t sel_sprite;
	uint8_t sprite_active;
	uint8_t sprite_waiting;
	uint8_t sprite_step;

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

	int mode;
	int dots;
	int ready;

	int x;
	uint8_t fetch_x;

	uint8_t fetcher_t;
	uint8_t fetcher_tile_id;
	uint8_t fetcher_bit_y;

	uint8_t window_active;
	uint8_t window_line;

	int mode3_cycles;
	int mode0_cycles;

	Bus *bus;

	uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wy, wx;
} PPU;

void init_ppu (PPU *ppu);
void init_ppu_reg (PPU *ppu);
void ppu_step (PPU *ppu, int cycles);

#endif
