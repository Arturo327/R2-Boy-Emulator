#include "cartucho/mbc2.h"
#include "gb.h"

uint8_t mbc2_read_rom (GB *gb, uint16_t addr)
{
	uint32_t offset = 0;

	if (addr < 0x4000) {
		offset = addr;

	} else if (addr < 0x8000) {
		uint32_t bank = gb->memory.cart.rom_bank & 0x0F;
		if (bank == 0) bank = 1;
		if (gb->memory.cart.rom_banks)
			bank &= (gb->memory.cart.rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < gb->memory.cart.rom_size)
		return gb->memory.cart.rom[offset];
	return 0xFF;
}

void mbc2_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	if (addr >= 0x4000) return;

	if (addr & 0x0100) {
		uint8_t bank = val & 0x0F;
		if (bank == 0) bank = 1;
		gb->memory.cart.rom_bank = bank;
	} else {
		gb->memory.cart.ram_enabled = ((val & 0x0F) == 0x0A);
	}
}

uint8_t mbc2_read_ram (GB *gb, uint16_t addr)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram)
		return 0xFF;

	uint16_t offset = (addr - 0xA000) & 0x01FF;
	return gb->memory.cart.ram[offset] | 0xF0;
}

void mbc2_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	if (!gb->memory.cart.ram_enabled || !gb->memory.cart.ram) return;

	uint16_t offset = (addr - 0xA000) & 0x01FF;
	gb->memory.cart.ram[offset] = val & 0x0F;
}
