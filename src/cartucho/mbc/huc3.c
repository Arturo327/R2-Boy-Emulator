#include "cartucho/mbc/huc3.h"
#include "gb.h"

#include <stdlib.h>
#include <string.h>

uint8_t huc3_read_rom (GB *gb, uint16_t addr)
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

void huc3_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	HuC3State *h = (HuC3State *) cart->state;

	if (addr < 0x2000) {
		h->select = val & 0x0F;

	} else if (addr < 0x4000) {
		cart->rom_bank = val & 0x7F;

	} else if (addr < 0x6000) {
		cart->ram_bank = val & 0x03;
	}
}

static uint16_t read_nibble_field (const uint8_t *mem, int base)
{
	return (uint16_t)((mem[base] & 0xF)
			| ((mem[base + 1] & 0xF) << 4)
			| ((mem[base + 2] & 0xF) << 8));
}

static void write_nibble_field (uint8_t *mem, int base, uint16_t val)
{
	mem[base] = val & 0xF;
	mem[base + 1] = (val >> 4) & 0xF;
	mem[base + 2] = (val >> 8) & 0xF;
}

static void huc3_sync (HuC3State *h)
{
	time_t now = time(NULL);
	long elapsed = (long) difftime(now, (time_t)h->base);
	if (elapsed <= 0) {
		h->base = (int64_t)now;
		return;
	}

	uint32_t minute = read_nibble_field(h->mem, 0x10) % 1440;
	uint32_t day = read_nibble_field(h->mem, 0x13);

	uint64_t total_minutes = (uint64_t) minute + (uint64_t)(elapsed / 60);
	day += (uint32_t)(total_minutes / 1440);
	minute = (uint32_t)(total_minutes % 1440);

	write_nibble_field(h->mem, 0x10, (uint16_t) minute);
	write_nibble_field(h->mem, 0x13, (uint16_t)(day & 0xFFF));

	h->base = (int64_t)(now - (elapsed % 60));
}

static void huc3_exec_extended (HuC3State *h, uint8_t sub)
{
	switch (sub)
	{
	case 0x0:
		huc3_sync(h);
		memcpy(h->mem, h->mem + 0x10, 6);
		h->mem[0x06] = 0;
		h->response = 1;
		break;

	case 0x1: {
		uint32_t old_total = read_nibble_field(h->mem, 0x10)
				+ read_nibble_field(h->mem, 0x13) * 1440u;
		uint32_t new_minute = read_nibble_field(h->mem, 0x00);
		uint32_t new_day = read_nibble_field(h->mem, 0x03);
		uint32_t new_total = new_minute + new_day * 1440u;

		int64_t delta = (int64_t) new_total - (int64_t) old_total;

		int64_t ev_total = (int64_t) read_nibble_field(h->mem, 0x58)
				+ (int64_t) read_nibble_field(h->mem, 0x5B) * 1440
				+ delta;
		if (ev_total < 0) ev_total = 0;

		const int64_t MAX_EV_MINUTES = (int64_t)0xFFF * 1440 + 1439;
		if (ev_total > MAX_EV_MINUTES) ev_total = MAX_EV_MINUTES;
		write_nibble_field(h->mem, 0x10, (uint16_t)(new_minute % 1440));
		write_nibble_field(h->mem, 0x13, (uint16_t) new_day);
		write_nibble_field(h->mem, 0x58, (uint16_t)(ev_total % 1440));
		write_nibble_field(h->mem, 0x5B, (uint16_t)(ev_total / 1440));

		h->base = (int64_t)time(NULL);
		h->response = 1;
		break;
	}
	case 0x2: h->response = 1; break;
	case 0xE: h->response = 0; break;
	default: h->response = 0; break;
	}
}

static void huc3_exec (HuC3State *h)
{
	switch (h->cmd)
	{
	case 0x1:
		huc3_sync(h);
		h->response = h->mem[h->addr] & 0x0F;
		h->addr = (h->addr + 1) & 0xFF;
		break;

	case 0x3:
		h->mem[h->addr] = h->arg & 0x0F;
		h->addr = (h->addr + 1) & 0xFF;
		h->response = 0;
		break;

	case 0x4:
		h->addr = (h->addr & 0xF0) | (h->arg & 0x0F);
		h->response = 0;
		break;

	case 0x5:
		h->addr = (h->addr & 0x0F) | ((h->arg & 0x0F) << 4);
		h->response = 0;
		break;

	case 0x6:
		huc3_exec_extended(h, h->arg & 0x0F);
		break;

	default: h->response = 0; break;
	}
}

uint8_t huc3_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	HuC3State *h = (HuC3State *) cart->state;

	switch (h->select)
	{
	case 0x0:
	case 0xA: {
		if (!cart->ram || cart->ram_size == 0) return 0xFF;
		uint32_t bank = cart->ram_bank;
		if (cart->ram_banks) bank &= (cart->ram_banks - 1);
		uint32_t offset = (bank << 13) + (addr - 0xA000);
		if (offset < cart->ram_size) return cart->ram[offset];
		return 0xFF;
	}

	case 0xC:
		return (uint8_t)(0x80 | ((h->cmd & 0x07) << 4) | (h->response & 0x0F));

	case 0xD:
		return (uint8_t)(0xFE | (h->ready & 0x01));

	case 0xE:
		return 0xC0;

	default:
		return 0xFF;
	}
}

void huc3_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	HuC3State *h = (HuC3State *) cart->state;

	switch (h->select)
	{
	case 0xA: {
		if (!cart->ram || cart->ram_size == 0) return;
		uint32_t bank = cart->ram_bank;
		if (cart->ram_banks) bank &= (cart->ram_banks - 1);
		uint32_t offset = (bank << 13) + (addr - 0xA000);

		pthread_mutex_lock(&gb->save.lock);
		if (offset < cart->ram_size) cart->ram[offset] = val;
		cart->save_needed = 1;
		pthread_mutex_unlock(&gb->save.lock);
		return;
	}

	case 0x0: return;

	case 0xB:
		pthread_mutex_lock(&gb->save.lock);
		h->cmd = (val >> 4) & 0x07;
		h->arg = val & 0x0F;
		pthread_mutex_unlock(&gb->save.lock);
		return;

	case 0xD:
		pthread_mutex_lock(&gb->save.lock);
		cart->save_needed = 1;
		if (!(val & 0x01))
			huc3_exec(h);
		pthread_mutex_unlock(&gb->save.lock);
		return;

	case 0xE:
		pthread_mutex_lock(&gb->save.lock);
		h->ir = val & 0x01;
		pthread_mutex_unlock(&gb->save.lock);
		return;

	default:
		return;
	}
}

int huc3_init (Cartucho *cart)
{
	cart->state = malloc(sizeof(HuC3State));
	if (!cart->state) return 0;

	memset(cart->state, 0, sizeof(HuC3State));
	HuC3State *h = (HuC3State *) cart->state;
	h->ready = 1;
	h->base = (int64_t)time(NULL);
	return 1;
}

void huc3_free (Cartucho *cart)
{
	free(cart->state);
	cart->state = NULL;
}
