#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <bus/bus.h>
#include "ppu/types.h"
#include "ppu/bg_fetcher.h"
#include "ppu/sp_fetcher.h"
#include "model.h"

void init_ppu (PPU *ppu, Model model);
void init_ppu_reg (PPU *ppu);
void shutdown_screen (PPU *ppu);
int ppu_shutdown_step (PPU *ppu);

int cgb_colors_active (PPU *ppu, GB *gb);
void check_lyc (PPU *ppu);
void update_stat_line (PPU *ppu);
void ppu_step (PPU *ppu);

#endif
