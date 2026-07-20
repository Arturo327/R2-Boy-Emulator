#include "cartucho/mbc/m161.h"
#include "gb.h"

uint8_t m161_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;

	uint32_t bank = cart->rom_bank & 0x07;
	uint32_t offset = (bank << 15) + addr;

	if (offset < cart->rom_size)
		return cart->rom[offset];

	return 0xFF;
}

void m161_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	(void) addr;

	if (cart->mbc_mode)
		return;

	cart->rom_bank = val & 0x07;
	cart->mbc_mode = 1;
}

uint8_t m161_read_ram (GB *gb, uint16_t addr)
{
	(void) gb;
	(void) addr;
	return 0xFF;
}

void m161_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	(void) gb;
	(void) addr;
	(void) val;
}
