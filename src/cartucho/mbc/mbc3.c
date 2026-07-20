#include "cartucho/mbc/mbc3.h"
#include "gb.h"

static void mbc3_sync (Cartucho *cart)
{
	RTC *rtc = (RTC *)cart->state;
	if (rtc->halt) {
		rtc->base = time(NULL);
		return;
	}

	time_t now = time(NULL);
	long elapsed = (long) difftime(now, rtc->base);
	if (elapsed <= 0) {
		rtc->base = now;
		return;
	}

	uint32_t total = (uint32_t)rtc->s
			+ (uint32_t)rtc->m * 60
			+ (uint32_t)rtc->h * 3600
			+ (uint32_t)rtc->d * 86400;
	total += (uint32_t) elapsed;

	uint32_t days = total / 86400;
	uint32_t rem = total % 86400;

	rtc->h = rem / 3600; rem %= 3600;
	rtc->m = rem / 60;
	rtc->s = rem % 60;

	if (days > 511) {
		rtc->carry = 1;
		days &= 0x1FF;
	}
	rtc->d = (uint16_t) days;

	rtc->base = now;
}

int rtc_init (Cartucho *cart)
{
	cart->state = malloc(sizeof(RTC));
	if (!cart->state) return 0;
	RTC *rtc = (RTC *)cart->state;
	rtc->latch_prev = 0xFF;
	rtc->base = time(NULL);
	return 1;
}	

void rtc_free (Cartucho *cart)
{
	free(cart->state);
	cart->state = NULL;
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
		pthread_mutex_lock(&gb->save.lock);
		RTC *rtc = (RTC *)cart->state;
		if (cart->has_rtc && rtc->latch_prev == 0x00 && val == 0x01) {
			mbc3_sync(cart);
			rtc->s_l = rtc->s;
			rtc->m_l = rtc->m;
			rtc->h_l = rtc->h;
			rtc->d_l = rtc->d;
			rtc->halt_l = rtc->halt;
			rtc->carry_l = rtc->carry;
		}
		rtc->latch_prev = val;
		cart->save_needed = 1;
		pthread_mutex_unlock(&gb->save.lock);
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

	RTC *rtc = (RTC *)cart->state;
	switch (cart->ram_bank)
	{
	case 0x08: return rtc->s_l;
	case 0x09: return rtc->m_l;
	case 0x0A: return rtc->h_l;
	case 0x0B: return (uint8_t)(rtc->d_l & 0xFF);
	case 0x0C: return (uint8_t)(((rtc->d_l >> 8) & 0x01)
			| (rtc->halt_l  ? 0x40 : 0)
			| (rtc->carry_l ? 0x80 : 0));
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

		pthread_mutex_lock(&gb->save.lock);
		if (offset < cart->ram_size)
			cart->ram[offset] = val;

		cart->save_needed = 1;
		pthread_mutex_unlock(&gb->save.lock);
		return;
	}

	if (!cart->has_rtc) return;

	pthread_mutex_lock(&gb->save.lock);
	mbc3_sync(cart);
	RTC *rtc = (RTC *)cart->state;
	switch (cart->ram_bank)
	{
	case 0x08: rtc->s = val & 0x3F; break;
	case 0x09: rtc->m = val & 0x3F; break;
	case 0x0A: rtc->h = val & 0x1F; break;
	case 0x0B: rtc->d = (rtc->d & 0x100) | val; break;
	case 0x0C: {
		rtc->d = (rtc->d & 0xFF) | ((uint16_t)(val & 0x01) << 8);
		rtc->halt = (val & 0x40) != 0;
		rtc->carry = (val & 0x80) != 0;
		break;
	}
	default: break;
	}
	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}
