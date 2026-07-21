#include "cartucho/mbc/huc1.h"
#include "gb.h"

uint8_t huc1_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	uint32_t offset;

	if (addr < 0x4000) {
		offset = addr;
	} else {
		uint32_t bank = cart->rom_bank;
		if (cart->rom_banks)
			bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void huc1_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x2000) {
		cart->mbc_mode = ((val & 0x0F) == 0x0E);

	} else if (addr < 0x4000) {
		uint8_t bank = val & 0x3F;
		if (bank == 0) bank = 1;
		cart->rom_bank = bank;

	} else if (addr < 0x6000) {
		cart->ram_bank = val & 0x03;
	}
}

uint8_t huc1_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;

	if (cart->mbc_mode) return 0xC0;

	if (!cart->ram || cart->ram_size == 0) return 0xFF;

	uint32_t bank = cart->ram_bank;
	if (cart->ram_banks)
		bank &= (cart->ram_banks - 1);

	uint32_t offset = (bank << 13) + (addr - 0xA000);
	if (offset < cart->ram_size)
		return cart->ram[offset];
	return 0xFF;
}

void huc1_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (cart->mbc_mode) return;

	if (!cart->ram || cart->ram_size == 0) return;

	uint32_t bank = cart->ram_bank;
	if (cart->ram_banks)
		bank &= (cart->ram_banks - 1);

	uint32_t offset = (bank << 13) + (addr - 0xA000);

	pthread_mutex_lock(&gb->save.lock);
	if (offset < cart->ram_size)
		cart->ram[offset] = val;
	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}
