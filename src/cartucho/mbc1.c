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

static inline uint32_t mbc1_bank_number (Cartucho *cart)
{
	uint8_t mask;
	uint8_t shift;
	if (cart->multicart) {
		mask = 0x0F;
		shift = 4;
	} else {
		mask = 0x1F;
		shift = 5;
	}

	/*
	uint8_t low = cart->bank1 & mask;
	if (low == 0) low = 1;
	*/
	uint8_t low = cart->bank1 & 0x1F;
	if (low == 0) low = 1;
	low &= mask;

	return ((uint32_t)cart->bank2 << shift) | low;
}


uint8_t mbc1_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	uint32_t offset = 0;

	if (addr < 0x4000) {
		uint32_t bank = 0;
		if (cart->mbc_mode == 1) {
			uint8_t shift = cart->multicart ? 4 : 5;
			bank = (uint32_t)cart->bank2 << shift;
			if (cart->rom_banks)
				bank &= (cart->rom_banks - 1);
		}
		offset = (bank << 14) + addr;

	} else if (addr < 0x8000) {
		uint32_t bank = mbc1_bank_number(cart);
		if (cart->rom_banks)
			bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void mbc1_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x2000) {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x4000) {
		cart->bank1 = val & 0x1F;

	} else if (addr < 0x6000) {
		cart->bank2 = val & 0x03;
		cart->ram_bank = cart->bank2;

	} else {
		cart->mbc_mode = val & 0x01;
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
