#include "cartucho/mbc/mbc5.h"
#include "gb.h"

uint8_t mbc5_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	uint32_t offset;

	if (addr < 0x4000) {
		offset = addr;

	} else {
		uint32_t bank = ((uint32_t)cart->bank2 << 8) | cart->bank1;
		if (cart->rom_banks)
			bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void mbc5_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x2000) {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x3000) {
		cart->bank1 = val;

	} else if (addr < 0x4000) {
		cart->bank2 = val & 0x01;

	} else if (addr < 0x6000) {
		if (cart->has_rumble) {
			uint8_t on = (val >> 3) & 0x01;
			if (on != cart->rumble_on) {
				if (cart->rumble_on)
					cart->rumble_on_ticks += (uint32_t)gb->clock - cart->rumble_since;

				cart->rumble_since = (uint32_t)gb->clock;
				cart->rumble_on = on;
			}
			cart->ram_bank = val & 0x07;

		} else {
			cart->ram_bank = val & 0x0F;
		}
	}
}

uint8_t mbc5_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram)
		return 0xFF;

	uint32_t bank = cart->ram_bank;
	if (cart->ram_banks)
		bank &= (cart->ram_banks - 1);

	uint32_t offset = (bank << 13) + (addr - 0xA000);
	if (offset < cart->ram_size)
		return cart->ram[offset];
	return 0xFF;
}

void mbc5_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram) return;

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

uint16_t mbc5_sample_rumble (GB *gb)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->has_rumble) return 0;

	uint32_t on_ticks = cart->rumble_on_ticks;
	if (cart->rumble_on)
		on_ticks += (uint32_t)gb->clock - cart->rumble_since;

	cart->rumble_on_ticks = 0;
	cart->rumble_since = 0;

	if (on_ticks == 0) return 0;
	if (on_ticks >= TICKS_PER_FRAME) return 0xFFFF;

	return (uint16_t)(((uint64_t)on_ticks * 0xFFFF) / TICKS_PER_FRAME);
}
