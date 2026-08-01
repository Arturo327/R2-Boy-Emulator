#include "bus/bus.h"
#include "gb.h"
#include "ppu/ppu.h"

static int timer_selected_bit (uint16_t div, uint8_t tac)
{
	switch (tac & 0x03)
	{
	case 0: return (div >> 9) & 1;
	case 1: return (div >> 3) & 1;
	case 2: return (div >> 5) & 1;
	case 3: return (div >> 7) & 1;
	default: return 0;
	}
}

static uint8_t joypad_calc_lo (GB *gb)
{
	uint8_t sel = gb->joypad.joyp & 0x30;
	uint8_t lo = 0x0F;

	if (!(sel & 0x20)) lo &= ~((gb->joypad.buttons >> 4) & 0x0F);
	if (!(sel & 0x10)) lo &= ~(gb->joypad.buttons & 0x0F);

	return lo;
}

static void joypad_interrupt (GB *gb, uint8_t old_lo, uint8_t new_lo)
{
	if (!((old_lo & ~new_lo) & 0x0F)) return;
	gb->interrupts.IF |= 0x10;
	gb->cpu.stopped = 0;
}

void joypad_update (GB *gb, uint8_t new_buttons)
{
	if (gb->joypad.buttons == new_buttons) return;

	uint8_t old_lo = joypad_calc_lo(gb);
	gb->joypad.buttons = new_buttons;
	uint8_t new_lo = joypad_calc_lo(gb);

	joypad_interrupt(gb, old_lo, new_lo);
}

void div_reset (GB *gb)
{
	uint8_t old_div = (uint8_t)(gb->timer.div >> 8);

	if (timer_selected_bit(gb->timer.div, gb->timer.tac) && (gb->timer.tac & 0x04)) {
		gb->timer.tima++;
		if (gb->timer.tima == 0)
			gb->timer.tima_overflow = 4;
	}

	gb->timer.div = 0;
	apu_div_reset(&gb->apu, old_div);

	if (serial_div_reset(&gb->serial, old_div))
		gb->interrupts.IF |= 0x08;
}

static void oam_bug_rw (GB *gb)
{
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

void oam_bug (GB *gb, uint16_t val, int is_write)
{
	if ((val & 0xFF00) != 0xFE00) return;
	if (gb->ppu.mode != OAM_SCAN) return;
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

uint8_t dma_read_source (GB *gb, uint16_t addr)
{
	if (addr < 0x8000) return gb->memory.cart.read_rom(gb, addr);
	if (addr < 0xA000) return gb->memory.vram[addr - 0x8000];
	if (addr < 0xC000) return gb->memory.cart.read_ram(gb, addr);
	if (addr < 0xE000) return gb->memory.wram[addr - 0xC000];
	return gb->memory.wram[addr - 0xE000];
}

static uint8_t read_cgb_regs (GB *gb, uint16_t addr)
{
	switch (addr)
	{
	case 0xFF72: return gb->memory.ff72;
	case 0xFF73: return gb->memory.ff73;
	case 0xFF75: return gb->memory.ff75 | 0x8F;
	case 0xFF76: return apu_pcm12(&gb->apu);
	case 0xFF77: return apu_pcm34(&gb->apu);
	case 0xFF68: return gb->ppu.bcps | 0x40;
	case 0xFF4F: return gb->memory.vram_bank | 0xFE;
	case 0xFF69:
		if (gb->ppu.mode == DRAWING) return 0xFF;
		return gb->memory.bg_palette_ram[gb->ppu.bcps & 0x3F];
	case 0xFF6A: return gb->ppu.ocps | 0x40;

	default: return 0xFF;
	}
}

static void write_cgb_regs (GB *gb, uint16_t addr, uint8_t val)
{
	switch (addr)
	{
	case 0xFF72: gb->memory.ff72 = val; break;
	case 0xFF73: gb->memory.ff73 = val; break;
	case 0xFF75: gb->memory.ff75 = val & 0x70; break;
	case 0xFF68: gb->ppu.bcps = val & 0xBF; break;
	case 0xFF4F: gb->memory.vram_bank = val & 0x01; break;
	case 0xFF70: gb->memory.wram_bank = val & 0x07; break;
	case 0xFF69:
		if (gb->ppu.mode != DRAWING)
			gb->memory.bg_palette_ram[gb->ppu.bcps & 0x3F] = val;
		if (gb->ppu.bcps & 0x80)
			gb->ppu.bcps = (gb->ppu.bcps & 0x80) | ((gb->ppu.bcps + 1) & 0x3F);
		break;

	case 0xFF6A: gb->ppu.ocps = val & 0xBF; break;
	case 0xFF6B:
		if (gb->ppu.mode != DRAWING)
			gb->memory.obj_palette_ram[gb->ppu.ocps & 0x3F] = val;
		if (gb->ppu.ocps & 0x80)
			gb->ppu.ocps = (gb->ppu.ocps & 0x80) | ((gb->ppu.ocps + 1) & 0x3F);
		break;
	case 0xFF6C: gb->ppu.opri = val; break;
	case 0xFF4C: if (gb->boot_rom_enabled) gb->memory.key0 = val & 0x04; break;
	case 0xFF4D: gb->memory.key1 = val & 0x01; break;
	}
}

static uint8_t bus_read8 (void *ctx, uint16_t addr)
{
	GB *gb = (GB *)ctx;

	if (gb->boot_rom_enabled) {
		if (addr < 0x100)
			return gb->memory.bios[addr];
		if (gb->model == CGB && addr >= 0x200 && addr < 0x900)
			return gb->memory.bios[addr];
	}

	if (addr < 0x8000) {
		return gb->memory.cart.read_rom(gb, addr);
	}

	if (addr < 0xA000) {
		if (gb->ppu.mode == DRAWING || gb->ppu.vram_pre_block) return 0xFF;
		return gb->memory.vram[addr - 0x8000];
	}

	if (addr < 0xC000) {
		return gb->memory.cart.read_ram(gb, addr);
	}

	if (addr < 0xE000) {
		return gb->memory.wram[addr - 0xC000];
	}

	if (addr < 0xFE00) {
		return gb->memory.wram[addr - 0xE000];
	}

	if (addr < 0xFEA0) {
		if (gb->dma.active) return 0xFF;
		if (gb->ppu.mode == OAM_SCAN || gb->ppu.mode == DRAWING || gb->ppu.oam_pre_block) return 0xFF;
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
		case 0xFF00: return (gb->joypad.joyp & 0x30) | joypad_calc_lo(gb) | 0xC0;

		// Serial Port
		case 0xFF01: return gb->serial.SB;
		case 0xFF02: return gb->serial.SC | 0x7E;

		// Timer
		case 0xFF04: return (uint8_t)(gb->timer.div >> 8);
		case 0xFF05: return gb->timer.tima;
		case 0xFF06: return gb->timer.tma;
		case 0xFF07: return gb->timer.tac | 0xF8;

		// Interrupts
		case 0xFF0F: return gb->interrupts.IF | 0xE0;

		// PPU
		case 0xFF40: return gb->ppu.lcdc;
		case 0xFF41: {
			gb->ppu.stat = (gb->ppu.stat & 0xFC) | gb->ppu.mode;
			return gb->ppu.stat | 0x80;
		}
		case 0xFF42: return gb->ppu.scy;
		case 0xFF43: return gb->ppu.scx;
		case 0xFF44: return gb->ppu.ly;
		case 0xFF45: return gb->ppu.lyc;
		case 0xFF46: return gb->ppu.dma;
		case 0xFF4A: return gb->ppu.wy;
		case 0xFF4B: return gb->ppu.wx;
		case 0xFF47: return gb->ppu.bgp;
		case 0xFF48: return gb->ppu.obp0;
		case 0xFF49: return gb->ppu.obp1;

		case 0xFF50: return 0xFF;

		default: if (gb->model == CGB) return read_cgb_regs(gb, addr); return 0xFF;
		}
	}

	if (addr < 0xFFFF) {
		return gb->memory.hram[addr - 0xFF80];
	}

	return gb->interrupts.IE;
}

static void bus_write8 (void *ctx, uint16_t addr, uint8_t val)
{
	GB *gb = (GB *)ctx;

	if (addr < 0x8000) {
		gb->memory.cart.write_rom(gb, addr, val);
		return;
	}

	if (addr < 0xA000) {
		if (gb->ppu.mode == DRAWING) return;
		gb->memory.vram[addr - 0x8000] = val;
		return;
	}

	if (addr < 0xC000) {
		gb->memory.cart.write_ram(gb, addr, val);
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
		if (gb->dma.active) return;
		if (gb->ppu.mode == DRAWING) return;
		if (gb->ppu.mode == OAM_SCAN && gb->ppu.oam_write_blocked) return;
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
			joypad_interrupt(gb, old_lo, new_lo);
			break;
		}

		// Serial Port
		case 0xFF01: gb->serial.SB = val; break;
		case 0xFF02: serial_write_sc(&gb->serial, val); break;

		// Timer
		case 0xFF04: div_reset(gb); break;
		case 0xFF05: {
			if (gb->timer.reload) {
				gb->timer.reload = 0;
			} else if (gb->timer.tima_overflow > 1) {
				gb->timer.tima = val;
				gb->timer.tima_overflow = 0;
			} else if (gb->timer.tima_overflow == 0) {
				gb->timer.tima = val;
			}
			break;
		}
		case 0xFF06: {
			gb->timer.tma = val;
			if (gb->timer.tima_overflow > 0 || gb->timer.reload) gb->timer.tima = val;
			if (gb->timer.reload) gb->timer.reload = 0;
			break;
		}
		case 0xFF07: {
			int and_before = timer_selected_bit(gb->timer.div, gb->timer.tac)
				& ((gb->timer.tac >> 2) & 1);
			gb->timer.tac = val;
			int and_after = timer_selected_bit(gb->timer.div, gb->timer.tac)
				& ((gb->timer.tac >> 2) & 1);
			if (and_before == 1 && and_after == 0) {
				gb->timer.tima++;
				if (gb->timer.tima == 0)
					gb->timer.tima_overflow = 4;
			}
			break;
		}

		// Interrupts
		case 0xFF0F: gb->interrupts.IF = (val & 0x1F) | 0xE0; break;

		// PPU
		case 0xFF40: gb->ppu.lcdc = val; break;
		case 0xFF41: {
			gb->ppu.stat = (gb->ppu.stat & 0x07) | (val & 0x78);
			update_stat_line(&gb->ppu);
			break;
		}
		case 0xFF42: gb->ppu.scy = val; break;
		case 0xFF43: gb->ppu.scx = val; break;
		case 0xFF44: break;
		case 0xFF45: {
			gb->ppu.lyc = val;
			if (gb->ppu.lcdc & 0x80)
				check_lyc(&gb->ppu);
			break;
		}
		case 0xFF46: {
			gb->ppu.dma = val;
			gb->dma.src = (uint16_t)val << 8;
			gb->dma.index = 0;
			gb->dma.delay = 2;
			break;
		}
		case 0xFF4A: gb->ppu.wy = val; break;
		case 0xFF4B: gb->ppu.wx = val; break;
		case 0xFF47: gb->ppu.bgp = val; break;
		case 0xFF48: gb->ppu.obp0 = val; break;
		case 0xFF49: gb->ppu.obp1 = val; break;

		case 0xFF50: gb->boot_rom_disable_pending = 1; break;

		default: if (gb->model == CGB) write_cgb_regs(gb, addr, val); break;
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
