#ifndef SP_FETCHER_H
#define SP_FETCHER_H

#include <stdint.h>
#include <bus/bus.h>
#include "ppu/types.h"

void start_sprites (PPU *ppu);
void sprite_fetch (PPU *ppu);
SpritePixel get_sp_pixel (PPU *ppu);

#endif
