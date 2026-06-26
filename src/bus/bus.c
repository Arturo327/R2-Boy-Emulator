#include "bus/bus.h"
#include "gb.h"
#include "ppu/ppu.h"

static uint8_t joypad_calc_lo (GB *gb) {
	uint8_t sel = gb->joypad.joyp & 0x30;
	uint8_t lo = 0x0F;

	if (!(sel & 0x20)) lo &= ~((gb->joypad.buttons >> 4) & 0x0F);
	if (!(sel & 0x10)) lo &= ~(gb->joypad.buttons & 0x0F);

	return lo;
}

void joypad_update (GB *gb, uint8_t new_buttons) {
	if (gb->joypad.buttons == new_buttons) return;

	uint8_t old_lo = joypad_calc_lo(gb);
	gb->joypad.buttons = new_buttons;
	uint8_t new_lo = joypad_calc_lo(gb);

	if ((old_lo & ~new_lo) & 0x0F) {
		gb->interrupts.IF |= 0x10;
	}
}

static void oam_bug_rw (GB *gb) {

	int row = gb->ppu.dots >> 2;
	if (row < 4 || row == 19) return;

	uint8_t *oam = gb->memory.oam;
	int curr = row << 3;
	int prev = (row - 1) << 3;
	int prev2 = (row - 2) << 3;

	uint16_t a = oam[prev2] | (oam[prev2 + 1] << 8);
	uint16_t b = oam[prev] | (oam[prev + 1] << 8);
	uint16_t c = oam[curr] | (oam[curr + 1] << 8);
	uint16_t d = oam[prev + 4] | (oam[prev + 5] << 8);

	uint16_t w = (b & (a | c | d)) | (a & c & d);

	oam[prev] = w & 0xFF;
	oam[prev + 1] = w >> 8;
	for (int i = 0; i < 8; i++) {
		uint8_t byte = oam[prev + i];
		oam[curr + i] = byte;
		oam[prev2 + i] = byte;
	}
}

void oam_bug (GB *gb, uint16_t val, int is_write) {

	if ((val & 0xFF00) != 0xFE00) return;
	if (gb->ppu.mode != 2) return;
	if (gb->ppu.dots >= 80) return;

	if (is_write == 2) {
		oam_bug_rw(gb);
		is_write = 0;
	}

	int row = gb->ppu.dots >> 2;
	if (row == 0) return;

	uint8_t *oam = gb->memory.oam;
	int curr = row << 3;
	int prev = (row - 1) << 3;

	uint16_t a = oam[curr] | (oam[curr + 1] << 8);
	uint16_t b = oam[prev] | (oam[prev + 1] << 8);
	uint16_t c = oam[prev + 4] | (oam[prev + 5] << 8);

	uint16_t w = is_write
		? ((a ^ c) & (b ^ c)) ^ c
		: b | (a & c);

	oam[curr] = w & 0xFF;
	oam[curr + 1] = w >> 8;
	for (int i = 2; i < 8; i++)
		oam[curr + i] = oam[prev + i];
}

uint8_t bus_read8 (void *ctx, uint16_t addr) {
	GB *gb = (GB *)ctx;

	if (addr < 0x100 && gb->boot_rom_enabled) {
		return gb->memory.bios[addr];
	}

	if (addr < 0x4000) {
		uint32_t bank = 0;
		if (gb->memory.cart.mbc_type == MBC1 &&
		    gb->memory.cart.mbc_mode == 1) {
			bank = gb->memory.cart.rom_bank & 0x60;
			if (gb->memory.cart.rom_banks)
				bank &= (gb->memory.cart.rom_banks - 1);
		}
		uint32_t offset = (bank << 14) + addr;
		if (offset < gb->memory.cart.rom_size)
			return gb->memory.cart.rom[offset];
		return 0xFF;

	}

	if (addr < 0x8000) {
		uint32_t bank = gb->memory.cart.rom_bank;
		if ((bank & 0x1F) == 0) bank |= 0x01;
		if (gb->memory.cart.rom_banks)
			bank &= (gb->memory.cart.rom_banks - 1);
		uint32_t offset = (bank << 14) + (addr - 0x4000);
		if (offset < gb->memory.cart.rom_size)
			return gb->memory.cart.rom[offset];
		return 0xFF;

	}

	if (addr < 0xA000) {
		return gb->memory.vram[addr - 0x8000];
	}

	if (addr < 0xC000) {
		if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram)
			return 0xFF;
		uint32_t ram_bank = gb->memory.cart.ram_bank;
		if (gb->memory.cart.mbc_type == MBC1 &&
		    gb->memory.cart.mbc_mode == 0)
			ram_bank = 0;
		if (gb->memory.cart.ram_banks > 0)
			ram_bank &= (gb->memory.cart.ram_banks - 1);
		uint32_t offset = (ram_bank << 13) + (addr - 0xA000);
		if (offset < gb->memory.cart.ram_size)
			return gb->memory.cart.ram[offset];
		return 0xFF;

	}

	if (addr < 0xE000) {
		return gb->memory.wram[addr - 0xC000];
	}

	if (addr < 0xFE00) {
		return gb->memory.wram[addr - 0xE000];
	}

	if (addr < 0xFEA0) {
		return gb->memory.oam[addr - 0xFE00];
	}

	if (addr < 0xFF00) {
		return 0xFF;
	}

	if (addr < 0xFF80) {

		if (addr >= 0xFF30 && addr <= 0xFF3F)
			return apu_wave_ram_read(&gb->apu, addr);

		if (addr >= 0xFF10 && addr <= 0xFF26)
			return apu_read_reg(&gb->apu, addr);

		switch (addr) {

			// Joypad
			case 0xFF00: {
				uint8_t sel = gb->joypad.joyp & 0x30;
				uint8_t lo = 0x0F;

				if (!(sel & 0x20)) {
					lo &= ~((gb->joypad.buttons >> 4) & 0x0F);
				}
				if (!(sel & 0x10)) {
					lo &= ~(gb->joypad.buttons & 0x0F);
				}
				return sel | lo | 0xC0;
			}

			// Serial Port
			case 0xFF01: return gb->joypad.SB;
			case 0xFF02: return gb->joypad.SC & 0x7F;

			// Timer
			case 0xFF04: return gb->timer.div;
			case 0xFF05: return gb->timer.tima;
			case 0xFF06: return gb->timer.tma;
			case 0xFF07: return gb->timer.tac;

			// Interrupts
			case 0xFF0F: return gb->interrupts.IF | 0xE0;

			// PPU
			case 0xFF40: return gb->ppu.lcdc;
			case 0xFF41: return gb->ppu.stat | 0x80;
			case 0xFF42: return gb->ppu.scy;
			case 0xFF43: return gb->ppu.scx;
			case 0xFF44: return gb->ppu.ly;
			case 0xFF45: return gb->ppu.lyc;
			case 0xFF46: return gb->ppu.dma;
			case 0xFF47: return gb->ppu.bgp;
			case 0xFF48: return gb->ppu.obp0;
			case 0xFF49: return gb->ppu.obp1;
			case 0xFF4A: return gb->ppu.wy;
			case 0xFF4B: return gb->ppu.wx;

			case 0xFF50: return 0xFF;

			default: return 0xFF;
		}
	}

	if (addr < 0xFFFF) {
		return gb->memory.hram[addr - 0xFF80];
	}

	return gb->interrupts.IE;
}

void bus_write8 (void *ctx, uint16_t addr, uint8_t val) {
	GB *gb = (GB *)ctx;

	if (addr < 0x8000) {
		if (gb->memory.cart.mbc_type == 0)
			return;
		if (gb->memory.cart.mbc_type == MBC1) {
			if (addr < 0x2000) {
				gb->memory.cart.ram_enabled = ((val & 0x0F) == 0x0A);
			}
			else if (addr < 0x4000) {
				uint8_t lo = val & 0x1F;
				if (lo == 0) lo = 1;
				gb->memory.cart.rom_bank =
					(gb->memory.cart.rom_bank & 0x60) | lo;
			}
			else if (addr < 0x6000) {
				uint8_t sec = val & 0x03;
				gb->memory.cart.ram_bank = sec;
				gb->memory.cart.rom_bank =
					(gb->memory.cart.rom_bank & 0x1F) | (sec << 5);
			}
			else {
				gb->memory.cart.mbc_mode = val & 0x01;
			}
			return;
		}

	}

	if (addr < 0xA000) {
		gb->memory.vram[addr - 0x8000] = val;
		return;
	}

	if (addr < 0xC000) {
		if (gb->memory.cart.ram_enabled && gb->memory.cart.ram) {
			uint32_t ram_bank = gb->memory.cart.ram_bank;
			if (gb->memory.cart.mbc_type == MBC1 &&
			    gb->memory.cart.mbc_mode == 0)
				ram_bank = 0;
			if (gb->memory.cart.ram_banks > 0)
				ram_bank &= (gb->memory.cart.ram_banks - 1);
			uint32_t offset = (ram_bank << 13) + (addr - 0xA000);
			if (offset < gb->memory.cart.ram_size)
				gb->memory.cart.ram[offset] = val;
		}
		return;

	}

	if (addr < 0xE000) {
		gb->memory.wram[addr - 0xC000] = val;
		return;
	}

	if (addr < 0xFE00) {
		gb->memory.wram[addr - 0xE000] = val;
		return;
	}

	if (addr < 0xFEA0) {
		gb->memory.oam[addr - 0xFE00] = val;
		return;
	}

	if (addr < 0xFF00) {
		return;
	}

	if (addr < 0xFF80) {

		if (addr >= 0xFF30 && addr <= 0xFF3F) {
			apu_wave_ram_write(&gb->apu, addr, val);
			return;
		}

		if (addr >= 0xFF10 && addr <= 0xFF26) {
			apu_write_reg(&gb->apu, addr, val);
			return;
		}

		switch (addr) {

			// Joypad
			case 0xFF00: {
				uint8_t old_lo = joypad_calc_lo(gb);
				gb->joypad.joyp = (gb->joypad.joyp & 0xCF) | (val & 0x30);
				uint8_t new_lo = joypad_calc_lo(gb);

				if ((old_lo & ~new_lo) & 0x0F) {
					gb->interrupts.IF |= 0x10;
				}
				break;
			}

			// Serial Port
			case 0xFF01: gb->joypad.SB = val; break;
			case 0xFF02: gb->joypad.SC = val; break;

			// Timer
			case 0xFF04: {
				uint8_t old_div = gb->timer.div;
				gb->timer.div = 0;
				gb->timer.div_counter = 0;
				gb->timer.tima_counter = 0;
				apu_div_reset(&gb->apu, old_div);
				break;
			}
			case 0xFF05: gb->timer.tima = val; break;
			case 0xFF06: gb->timer.tma = val; break;
			case 0xFF07: gb->timer.tac = val; break;

			case 0xFF0F: gb->interrupts.IF = (val & 0x1F) | 0xE0; break;

			// PPU
			case 0xFF40: gb->ppu.lcdc = val; break;
			case 0xFF41: {
				gb->ppu.stat = (gb->ppu.stat & 0x07) | (val & 0x78);
				break;
			}
			case 0xFF42: gb->ppu.scy = val; break;
			case 0xFF43: gb->ppu.scx = val; break;
			case 0xFF44: break;
			case 0xFF45: {
				gb->ppu.lyc = val;
				check_lyc(&gb->ppu);
				break;
			}
			case 0xFF46: {
				gb->ppu.dma = val;
				gb->dma_active = 1;
				gb->dma_src = (uint16_t)val << 8;
				gb->dma_index = 0;
				break;
			}
			case 0xFF47: gb->ppu.bgp = val; break;
			case 0xFF48: gb->ppu.obp0 = val; break;
			case 0xFF49: gb->ppu.obp1 = val; break;
			case 0xFF4A: gb->ppu.wy = val; break;
			case 0xFF4B: gb->ppu.wx = val; break;

			case 0xFF50: gb->boot_rom_disable_pending = 1; break;

			default: break;
		}
		return;
	}

	if (addr < 0xFFFF) {
		gb->memory.hram[addr - 0xFF80] = val;
		return;
	}

	if (addr == 0xFFFF) {
		gb->interrupts.IE = val;
	}
}

void init_bus (Bus *bus, GB *gb) {
	gb->cpu.bus = bus;
	gb->ppu.bus = bus;

	gb->interrupts.IME = 0;
	gb->interrupts.IE = 0;
	gb->interrupts.IF = 0xE1;
	gb->interrupts.ei_pending = 0;

	bus->ctx = (void *) gb;
	bus->read8 = bus_read8;
	bus->write8 = bus_write8;
	bus->opcodes = &gb->opcodes;
	bus->interrupts = &gb->interrupts;
}
