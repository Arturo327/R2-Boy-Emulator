#include "cartucho/mbc3.h"
#include "gb.h"

static void mbc3_sync (Cartucho *cart)
{
	if (cart->rtc.halt) {
		cart->rtc.base = time(NULL);
		return;
	}

	time_t now = time(NULL);
	long elapsed = (long) difftime(now, cart->rtc.base);
	if (elapsed <= 0) {
		cart->rtc.base = now;
		return;
	}

	uint32_t total = (uint32_t)cart->rtc.s
			+ (uint32_t)cart->rtc.m * 60
			+ (uint32_t)cart->rtc.h * 3600
			+ (uint32_t)cart->rtc.d * 86400;
	total += (uint32_t) elapsed;

	uint32_t days = total / 86400;
	uint32_t rem = total % 86400;

	cart->rtc.h = rem / 3600; rem %= 3600;
	cart->rtc.m = rem / 60;
	cart->rtc.s = rem % 60;

	if (days > 511) {
		cart->rtc.carry = 1;
		days &= 0x1FF;
	}
	cart->rtc.d = (uint16_t) days;

	cart->rtc.base = now;
}

uint8_t mbc3_read_rom (GB *gb, uint16_t addr)
{
	uint32_t offset = 0;
	Cartucho *cart = &gb->memory.cart;

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

void mbc3_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;

	if (addr < 0x2000) {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x4000) {
		uint8_t bank = cart->mbc30 ? val : (val & 0x7F);
		if (bank == 0) bank = 1;
		cart->rom_bank = bank;

	} else if (addr < 0x6000) {
		cart->ram_bank = val;

	} else {
		if (cart->has_rtc && cart->rtc.latch_prev == 0x00 && val == 0x01) {
			mbc3_sync(cart);
			cart->rtc.s_l = cart->rtc.s;
			cart->rtc.m_l = cart->rtc.m;
			cart->rtc.h_l = cart->rtc.h;
			cart->rtc.d_l = cart->rtc.d;
			cart->rtc.halt_l = cart->rtc.halt;
			cart->rtc.carry_l = cart->rtc.carry;
		}
		cart->rtc.latch_prev = val;
	}
}

uint8_t mbc3_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled) return 0xFF;

	if (cart->ram_bank <= 0x07) {
		if (!cart->ram) return 0xFF;
		uint32_t bank = cart->ram_bank;
		if (cart->ram_banks) bank &= (cart->ram_banks - 1);
		uint32_t offset = (bank << 13) + (addr - 0xA000);
		if (offset < cart->ram_size)
			return cart->ram[offset];
		return 0xFF;
	}

	if (!cart->has_rtc) return 0xFF;

	switch (cart->ram_bank) {
		case 0x08: return cart->rtc.s_l;
		case 0x09: return cart->rtc.m_l;
		case 0x0A: return cart->rtc.h_l;
		case 0x0B: return (uint8_t)(cart->rtc.d_l & 0xFF);
		case 0x0C: return (uint8_t)(((cart->rtc.d_l >> 8) & 0x01)
				| (cart->rtc.halt_l  ? 0x40 : 0)
				| (cart->rtc.carry_l ? 0x80 : 0));
		default: return 0xFF;
	}
}

void mbc3_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled) return;

	if (cart->ram_bank <= 0x07) {
		if (!cart->ram) return;
		uint32_t bank = cart->ram_bank;
		if (cart->ram_banks) bank &= (cart->ram_banks - 1);
		uint32_t offset = (bank << 13) + (addr - 0xA000);
		if (offset < cart->ram_size)
			cart->ram[offset] = val;
		return;
	}

	if (!cart->has_rtc) return;

	mbc3_sync(cart);
	switch (cart->ram_bank) {
		case 0x08: cart->rtc.s = val & 0x3F; break;
		case 0x09: cart->rtc.m = val & 0x3F; break;
		case 0x0A: cart->rtc.h = val & 0x1F; break;
		case 0x0B: cart->rtc.d = (cart->rtc.d & 0x100) | val; break;
		case 0x0C:
			cart->rtc.d = (cart->rtc.d & 0xFF) | ((uint16_t)(val & 0x01) << 8);
			cart->rtc.halt = (val & 0x40) != 0;
			cart->rtc.carry = (val & 0x80) != 0;
			break;
		default: break;
	}
}
