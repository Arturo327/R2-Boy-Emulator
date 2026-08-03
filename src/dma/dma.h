#ifndef DMA_H
#define DMA_H

#include <stdint.h>

typedef struct GB GB;

typedef struct DMA {
	uint8_t active;
	uint16_t src;
	uint8_t index;
	uint8_t delay;
} DMA;

typedef struct HDMA {
	uint16_t src;
	uint16_t dst;
	uint16_t length;
	uint8_t active;
	uint16_t stall;
} HDMA;

void dma_step (GB *gb);

void hdma_write_ff55 (GB *gb, uint8_t val);
uint8_t hdma_read_ff55 (GB *gb);
void hdma_hblank_step (GB *gb);
void hdma_gdma_step (GB *gb);

#endif
