#include "cartucho/mbc1.h"
#include "gb.h"

uint8_t mbcNone_read_rom (GB *gb, uint16_t addr)
{
	if (addr < gb->memory.cart.rom_size)
		return gb->memory.cart.rom[addr];
	return 0xFF;
}

void mbcNone_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	(void) gb;
	(void) addr;
	(void) val;
	return;
}

uint8_t mbcNone_read_ram (GB *gb, uint16_t addr)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram)
		return 0xFF;

	uint16_t offset = addr - 0xA000;

	if (offset < gb->memory.cart.ram_size)
		return gb->memory.cart.ram[offset];

	return 0xFF;
}

void mbcNone_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram) return;

	uint16_t offset = addr - 0xA000;

	if (offset < gb->memory.cart.ram_size)
		gb->memory.cart.ram[offset] = val;
}

uint8_t mbc1_read_rom (GB *gb, uint16_t addr)
{
	uint32_t offset = 0;
	if (addr < 0x4000) {
		uint32_t bank = 0;
		if (gb->memory.cart.mbc_mode == 1) {
			bank = gb->memory.cart.rom_bank & 0x60;
			if (gb->memory.cart.rom_banks)
				bank &= (gb->memory.cart.rom_banks - 1);
		}
		offset = (bank << 14) + addr;

	} else if (addr < 0x8000) {
		uint32_t bank = gb->memory.cart.rom_bank;
		if ((bank & 0x1F) == 0) bank |= 0x01;
		if (gb->memory.cart.rom_banks)
			bank &= (gb->memory.cart.rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < gb->memory.cart.rom_size)
		return gb->memory.cart.rom[offset];
	return 0xFF;
}

void mbc1_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	if (addr < 0x2000) {
		gb->memory.cart.ram_enabled = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x4000) {
		uint8_t l = val & 0x1F;
		if (l == 0) l = 1;
		gb->memory.cart.rom_bank = (gb->memory.cart.rom_bank & 0x60) | l;

	} else if (addr < 0x6000) {
		uint8_t sec = val & 0x03;
		gb->memory.cart.ram_bank = sec;
		gb->memory.cart.rom_bank = (gb->memory.cart.rom_bank & 0x1F) | (sec << 5);

	} else {
		gb->memory.cart.mbc_mode = val & 0x01;
	}
}

uint8_t mbc1_read_ram (GB *gb, uint16_t addr)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram)
		return 0xFF;

	uint32_t ram_bank = gb->memory.cart.ram_bank;
	if (gb->memory.cart.mbc_mode == 0)
		ram_bank = 0;
	if (gb->memory.cart.ram_banks > 0)
		ram_bank &= (gb->memory.cart.ram_banks - 1);

	uint32_t offset = (ram_bank << 13) + (addr - 0xA000);
	if (offset < gb->memory.cart.ram_size)
		return gb->memory.cart.ram[offset];
	return 0xFF;
}

void mbc1_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram) return;

	uint32_t ram_bank = gb->memory.cart.ram_bank;
	if (gb->memory.cart.mbc_mode == 0) ram_bank = 0;
	if (gb->memory.cart.ram_banks > 0) ram_bank &= (gb->memory.cart.ram_banks - 1);

	uint32_t offset = (ram_bank << 13) + (addr - 0xA000);
	if (offset < gb->memory.cart.ram_size)
		gb->memory.cart.ram[offset] = val;
}
