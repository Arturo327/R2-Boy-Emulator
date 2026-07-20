#include "cartucho/mbc/none.h"
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
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram)
		return 0xFF;

	uint16_t offset = addr - 0xA000;

	if (offset < cart->ram_size)
		return cart->ram[offset];

	return 0xFF;
}

void mbcNone_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram) return;

	uint16_t offset = addr - 0xA000;

	pthread_mutex_lock(&gb->save.lock);
	if (offset < cart->ram_size)
		cart->ram[offset] = val;

	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}
