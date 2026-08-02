#ifndef BG_FETCHER_H
#define BG_FETCHER_H

#include <stdint.h>
#include <bus/bus.h>
#include "ppu/types.h"

BgPixel bg_fifo_pop (PPU *ppu);
void bg_fetch (PPU *ppu);

#endif
