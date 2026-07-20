#include "cartucho/mbc/mmm01.h"
#include "gb.h"

int mmm01_init (Cartucho *cart)
{
	cart->state = malloc(sizeof(MMM01Regs));
	if (!cart->state) return 0;
	memset(cart->state, 0, sizeof(MMM01Regs));
	return 1;
}

void mmm01_free (Cartucho *cart)
{
	free(cart->state);
	cart->state = NULL;
}

static uint32_t mmm01_rom_bank_number (Cartucho *cart, uint16_t addr)
{
	MMM01Regs *m = (MMM01Regs *)cart->state;

	uint8_t high = m->rom_bank_high & 0x03;
	uint8_t mid;
	uint8_t low;

	if (addr < 0x4000) {
		low = m->rom_bank_low & m->rom_bank_mask;

		if (m->multiplex)
			mid = m->mbc1_mode ? m->ram_bank_low : (m->ram_bank_low & m->ram_bank_mask);
		else
			mid = m->rom_bank_mid;

	} else {
		low = m->rom_bank_low;
		if ((low & ~m->rom_bank_mask & 0x1F) == 0)
			low |= 1;

		mid = m->multiplex ? m->ram_bank_low : m->rom_bank_mid;
	}

	uint32_t bank = ((uint32_t)high << 7) | ((uint32_t)mid << 5) | low;
	if (cart->rom_banks) bank &= (cart->rom_banks - 1);
	return bank;
}

static uint32_t mmm01_ram_bank_number (Cartucho *cart)
{
	MMM01Regs *m = (MMM01Regs *)cart->state;
	uint8_t low;

	if (m->multiplex) {
		low = m->rom_bank_mid & 0x03;
	} else {
		low = m->mbc1_mode ? m->ram_bank_low : (m->ram_bank_low & m->ram_bank_mask);
	}

	uint8_t bank = ((m->ram_bank_high & 0x03) << 2) | (low & 0x03);
	if (cart->ram_banks) bank &= (cart->ram_banks - 1);
	return bank;
}

uint8_t mmm01_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	MMM01Regs *m = (MMM01Regs *)cart->state;
	uint32_t offset;

	if (!m->mapped) {
		uint32_t base = (cart->rom_size >= 0x8000) ? (cart->rom_size - 0x8000) : 0;
		offset = base + addr;
	} else {
		uint32_t bank = mmm01_rom_bank_number(cart, addr);
		uint16_t low = (addr < 0x4000) ? addr : (addr - 0x4000);
		offset = (bank << 14) + low;
	}

	if (offset < cart->rom_size)
		return cart->rom[offset];
	return 0xFF;
}

void mmm01_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	MMM01Regs *m = (MMM01Regs *)cart->state;

	if (addr < 0x2000) {
		cart->ram_enabled = ((val & 0x0F) == 0x0A);

		if (!m->mapped) {
			m->ram_bank_mask = (val >> 4) & 0x03;
			if (val & 0x40) m->mapped = 1;
		}

	} else if (addr < 0x4000) {
		uint8_t new_low = val & 0x1F;
		m->rom_bank_low = (m->rom_bank_low & m->rom_bank_mask)
				| (new_low & ~m->rom_bank_mask & 0x1F);

		if (!m->mapped)
			m->rom_bank_mid = (val >> 5) & 0x03;

	} else if (addr < 0x6000) {
		uint8_t new_low = val & 0x03;
		m->ram_bank_low = (m->ram_bank_low & m->ram_bank_mask)
				| (new_low & ~m->ram_bank_mask & 0x03);

		if (!m->mapped) {
			m->ram_bank_high = (val >> 2) & 0x03;
			m->rom_bank_high = (val >> 4) & 0x03;
			m->mode_write_disable = (val >> 6) & 0x01;
		}

	} else if (addr < 0x8000) {
		if (!m->mode_write_disable)
			m->mbc1_mode = val & 0x01;

		if (!m->mapped) {
			m->rom_bank_mask = ((val >> 1) & 0x1F) & ~0x01;
			m->multiplex = (val >> 6) & 0x01;
		}
	}
}

uint8_t mmm01_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram) return 0xFF;

	uint32_t bank = mmm01_ram_bank_number(cart);
	uint32_t offset = (bank << 13) + (addr - 0xA000);

	if (offset < cart->ram_size)
		return cart->ram[offset];
	return 0xFF;
}

void mmm01_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	if (!cart->ram_enabled || !cart->ram) return;

	uint32_t bank = mmm01_ram_bank_number(cart);
	uint32_t offset = (bank << 13) + (addr - 0xA000);

	pthread_mutex_lock(&gb->save.lock);
	if (offset < cart->ram_size)
		cart->ram[offset] = val;

	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}
