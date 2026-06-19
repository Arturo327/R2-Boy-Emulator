#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <bus/bus.h>
#include "ppu/types.h"
#include "ppu/bg_fetcher.h"
#include "ppu/sp_fetcher.h"

void init_ppu (PPU *ppu);
void init_ppu_reg (PPU *ppu);

void check_lyc (PPU *ppu);
void ppu_step (PPU *ppu);

#endif
