#include "dma/dma.h"
#include "gb.h"

#define HDMA_BLOCK_STALL_DOTS 32

void dma_step (GB *gb)
{
	if (gb->dma.delay > 0) {
		gb->dma.delay--;
		if (gb->dma.delay == 0)
			gb->dma.active = 1;

	} else if (gb->dma.active) {
		gb->memory.oam[gb->dma.index] = dma_read_source(gb, gb->dma.src + gb->dma.index);
		gb->dma.index++;
		if (gb->dma.index >= 0xA0)
			gb->dma.active = 0;
	}
}

static int hdma_copy_block (GB *gb)
{
	if (gb->hdma.dst + 16 > 0x2000) {
		gb->hdma.active = 0;
		return 0;
	}

	for (int i = 0; i < 16; i++) {
		uint8_t val = dma_read_source(gb, gb->hdma.src + i);
		gb->memory.vram[vram_map(gb, gb->hdma.dst + i)] = val;
	}

	gb->hdma.src += 16;
	gb->hdma.dst += 16;
	gb->hdma.length -= 16;
	if (gb->hdma.length == 0) gb->hdma.active = 0;
	return 1;
}

void hdma_gdma_step (GB *gb)
{
	uint16_t blocks_done = 0;

	while (gb->hdma.length > 0) {
		if (!hdma_copy_block(gb)) break;
		blocks_done++;
	}

	gb->hdma.stall = HDMA_BLOCK_STALL_DOTS * blocks_done;
}

void hdma_hblank_step (GB *gb)
{
	if (!gb->hdma.active || gb->cpu.halted) return;

	if (hdma_copy_block(gb))
		gb->hdma.stall = HDMA_BLOCK_STALL_DOTS;
}

uint8_t hdma_read_ff55 (GB *gb)
{
	uint8_t remaining = (uint8_t)((gb->hdma.length >> 4) - 1);
	return (gb->hdma.active ? 0x00 : 0x80) | (remaining & 0x7F);
}

void hdma_write_ff55 (GB *gb, uint8_t val)
{
	if (gb->hdma.active && !(val & 0x80)) {
		gb->hdma.active = 0;
		return;
	}

	gb->hdma.length = ((uint16_t)(val & 0x7F) + 1) << 4;

	if (val & 0x80) {
		gb->hdma.active = 1;
		return;
	}

	hdma_gdma_step(gb);
}
