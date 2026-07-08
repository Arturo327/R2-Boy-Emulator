#include "cartucho/mbc2.h"
#include "gb.h"

uint8_t mbc2_read_rom (GB *gb, uint16_t addr)
{
	uint32_t offset = 0;
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x4000) {
		offset = addr;

	} else if (addr < 0x8000) {
		uint32_t bank = cart->rom_bank & 0x0F;
		if (bank == 0) bank = 1;
		if (cart->rom_banks)
			bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void mbc2_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (addr >= 0x4000) return;

	if (addr & 0x0100) {
		uint8_t bank = val & 0x0F;
		if (bank == 0) bank = 1;
		cart->rom_bank = bank;
	} else {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);
	}
}

uint8_t mbc2_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram)
		return 0xFF;

	uint16_t offset = (addr - 0xA000) & 0x01FF;
	return cart->ram[offset] | 0xF0;
}

void mbc2_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram) return;

	uint16_t offset = (addr - 0xA000) & 0x01FF;
	cart->ram[offset] = val & 0x0F;
}
