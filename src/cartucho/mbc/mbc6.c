#include "cartucho/mbc/mbc6.h"
#include "gb.h"
 
#include <stdlib.h>
#include <string.h>

int mbc6_init (Cartucho *cart)
{
	cart->mbc6.flash.data = malloc(MBC6_FLASH_SIZE);
	if (!cart->mbc6.flash.data) return 0;

	memset(cart->mbc6.flash.data, 0xFF, MBC6_FLASH_SIZE);
	memset(cart->mbc6.flash.hidden, 0xFF, sizeof(cart->mbc6.flash.hidden));
	return 1;
}

void mbc6_free (Cartucho *cart)
{
	free(cart->mbc6.flash.data);
	cart->mbc6.flash.data = NULL;
}

static inline uint32_t rom_offset (Cartucho *cart, uint8_t bank, uint16_t addr, uint16_t base)
{
	uint32_t banks_8k = cart->rom_size >> 13;
	uint32_t b = bank;
	if (banks_8k) b &= (banks_8k - 1);
	return (b << 13) + (addr - base);
}

static inline uint32_t flash_offset (uint8_t bank, uint16_t addr, uint16_t base) {
	return (((uint32_t)bank & 0x7F) << 13) + (addr - base);
}

static inline void flash_idle (MBC6Flash *f) {
	f->state = MF_IDLE;
	f->program_len = 0;
}

static void flash_start_program (MBC6Flash *f, uint8_t hidden) {
	f->program_hidden = hidden;
	f->program_len = 0;
	f->state = MF_PROGRAM;
}

static void flash_erase_sector (MBC6Flash *f, uint32_t addr)
{
	uint32_t sector = addr & ~MBC6_SECTOR_MASK;
	if (sector == 0 && (f->sector0_locked || !f->write_enable)) { flash_idle(f); return; }
	memset(f->data + sector, 0xFF, MBC6_SECTOR_SIZE);
	f->status = 0x80;
	f->state = MF_STATUS;
}

static void flash_erase_chip (MBC6Flash *f)
{
	uint32_t start = (f->sector0_locked || !f->write_enable) ? MBC6_SECTOR_SIZE : 0;
	memset(f->data + start, 0xFF, MBC6_FLASH_SIZE - start);
	f->status = 0x80;
	f->state = MF_STATUS;
}

static void flash_erase_hidden (MBC6Flash *f)
{
	if (f->write_enable)
		memset(f->hidden, 0xFF, sizeof(f->hidden));
	f->status = 0x80;
	f->state = MF_STATUS;
}

static void flash_write (MBC6Flash *f, uint32_t addr, uint8_t val)
{
	switch (f->state)
	{
	case MF_IDLE:
		if (val == 0xAA && addr == 0x5555) f->state = MF_UNLOCK1;
		return;

	case MF_UNLOCK1:
		if (val == 0x55 && addr == 0x2AAA) f->state = MF_UNLOCK2;
		else flash_idle(f);
		return;

	case MF_UNLOCK2:
		switch (val)
		{
		case 0x80: f->state = MF_ERASE_UNLOCK1; return;
		case 0x60: f->state = MF_PROTECT_UNLOCK1; return;
		case 0x77: f->state = MF_HIDDEN_UNLOCK1; return;
		case 0x90: f->state = MF_ID_MODE; return;
		case 0xA0: flash_start_program(f, 0); return;
		default: flash_idle(f); return;
		}

	case MF_ERASE_UNLOCK1:
		if (val == 0xAA && addr == 0x5555) f->state = MF_ERASE_UNLOCK2;
		else flash_idle(f);
		return;
	case MF_ERASE_UNLOCK2:
		if (val == 0x55 && addr == 0x2AAA) f->state = MF_ERASE_UNLOCK3;
		else flash_idle(f);
		return;
	case MF_ERASE_UNLOCK3:
		if (val == 0x30) flash_erase_sector(f, addr);
		else if (val == 0x10) flash_erase_chip(f);
		else flash_idle(f);
		return;
	case MF_PROTECT_UNLOCK1:
		if (val == 0xAA && addr == 0x5555) f->state = MF_PROTECT_UNLOCK2;
		else flash_idle(f);
		return;
	case MF_PROTECT_UNLOCK2:
		if (val == 0x55 && addr == 0x2AAA) f->state = MF_PROTECT_UNLOCK3;
		else flash_idle(f);
		return;
	case MF_PROTECT_UNLOCK3:
		switch (val)
		{
		case 0x04: flash_erase_hidden(f); return;
		case 0xE0: flash_start_program(f, 1); return;
		case 0x40:
			if (f->write_enable) f->sector0_locked = 0;
			f->status = 0x80 | (f->sector0_locked ? 0x02 : 0x00);
			f->state = MF_STATUS;
			return;
		case 0x20:
			if (f->write_enable) f->sector0_locked = 1;
			f->status = 0x80 | (f->sector0_locked ? 0x02 : 0x00);
			f->state = MF_STATUS;
			return;
		default: flash_idle(f); return;
		}

	case MF_HIDDEN_UNLOCK1:
		if (val == 0xAA && addr == 0x5555) f->state = MF_HIDDEN_UNLOCK2;
		else flash_idle(f);
		return;
	case MF_HIDDEN_UNLOCK2:
		if (val == 0x55 && addr == 0x2AAA) f->state = MF_HIDDEN_UNLOCK3;
		else flash_idle(f);
		return;
	case MF_HIDDEN_UNLOCK3:
		if (val == 0x77) f->state = MF_READ_HIDDEN;
		else flash_idle(f);
		return;

	case MF_PROGRAM:
		if (f->program_len == 0)
			f->program_addr = f->program_hidden ? ((addr & 0xFF) & ~0x7Fu) : (addr & ~0x7Fu);

		if (f->program_len < 128)
			f->program_buf[f->program_len++] = val;

		if (f->program_len >= 128) {
			uint8_t needs_we = f->program_hidden || (f->program_addr < MBC6_SECTOR_SIZE);
			if (!needs_we || f->write_enable) {
				uint8_t *dst = f->program_hidden
					? f->hidden + f->program_addr
					: f->data + (f->program_addr & (MBC6_FLASH_SIZE - 1));
				for (int i = 0; i < 128; i++)
					dst[i] &= f->program_buf[i];
			}
			f->status = 0x80;
			f->state = MF_STATUS;
		}
		return;

	case MF_ID_MODE:
	case MF_READ_HIDDEN:
	case MF_STATUS:
		if (val == 0xF0) flash_idle(f);
		return;
	}
}

static uint8_t flash_read (MBC6Flash *f, uint32_t addr)
{
	switch (f->state)
	{
	case MF_ID_MODE:
		return (addr & 1) ? 0x81 : 0xC2;

	case MF_READ_HIDDEN:
		return f->hidden[addr & 0xFF];

	case MF_STATUS:
		return f->status;

	default:
		return f->data[addr & (MBC6_FLASH_SIZE - 1)];
	}
}

uint8_t mbc6_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	MBC6State *m = &cart->mbc6;

	if (addr < 0x4000)
		return (addr < cart->rom_size) ? cart->rom[addr] : 0xFF;

	if (addr < 0x6000) {
		if (m->flash_sel_a) {
			if (!m->flash_enabled) return 0xFF;
			return flash_read(&m->flash, flash_offset(m->rom_bank_a, addr, 0x4000));
		}
		uint32_t off = rom_offset(cart, m->rom_bank_a, addr, 0x4000);
		return (off < cart->rom_size) ? cart->rom[off] : 0xFF;
	}

	if (m->flash_sel_b) {
		if (!m->flash_enabled) return 0xFF;
		return flash_read(&m->flash, flash_offset(m->rom_bank_b, addr, 0x6000));
	}
	uint32_t off = rom_offset(cart, m->rom_bank_b, addr, 0x6000);
	return (off < cart->rom_size) ? cart->rom[off] : 0xFF;
}

void mbc6_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	MBC6State *m = &cart->mbc6;

	if (addr < 0x0400) {
		m->ram_enabled = ((val & 0x0F) == 0x0A);
		return;
	}
	if (addr < 0x0800) {
		m->ram_bank_a = val & 0x07;
		return;
	}
	if (addr < 0x0C00) {
		m->ram_bank_b = val & 0x07;
		return;
	}
	if (addr < 0x1000) {
		m->flash_enabled = val & 0x01;
		return;
	}
	if (addr < 0x2000) {
		m->flash_write_enabled = val & 0x01;
		m->flash.write_enable = m->flash_write_enabled;
		return;
	}
	if (addr < 0x2800) {
		m->rom_bank_a = val & 0x7F;
		return;
	}
	if (addr < 0x3000) {
		m->flash_sel_a = (val & 0x08) != 0;
		return;
	}
	if (addr < 0x3800) {
		m->rom_bank_b = val & 0x7F;
		return;
	}
	if (addr < 0x4000) {
		m->flash_sel_b = (val & 0x08) != 0;
		return;
	}

	if (addr < 0x6000) {
		if (!m->flash_sel_a || !m->flash_enabled) return;
		flash_write(&m->flash, flash_offset(m->rom_bank_a, addr, 0x4000), val);
		return;
	}

	if (!m->flash_sel_b || !m->flash_enabled) return;
	flash_write(&m->flash, flash_offset(m->rom_bank_b, addr, 0x6000), val);
}

static uint32_t ram_offset (Cartucho *cart, uint8_t bank, uint16_t addr, uint16_t base)
{
	uint32_t banks_4k = cart->ram_size >> 12;
	uint32_t b = bank;
	if (banks_4k) b &= (banks_4k - 1);
	return (b << 12) + (addr - base);
}

uint8_t mbc6_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	MBC6State *m = &cart->mbc6;
	if (!m->ram_enabled || !cart->ram) return 0xFF;

	uint8_t bank = (addr < 0xB000) ? m->ram_bank_a : m->ram_bank_b;
	uint16_t base = (addr < 0xB000) ? 0xA000 : 0xB000;
	uint32_t offset = ram_offset(cart, bank, addr, base);

	return (offset < cart->ram_size) ? cart->ram[offset] : 0xFF;
}

void mbc6_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	MBC6State *m = &cart->mbc6;
	if (!m->ram_enabled || !cart->ram) return;

	uint8_t bank = (addr < 0xB000) ? m->ram_bank_a : m->ram_bank_b;
	uint16_t base = (addr < 0xB000) ? 0xA000 : 0xB000;
	uint32_t offset = ram_offset(cart, bank, addr, base);

	pthread_mutex_lock(&gb->save.lock);
	if (offset < cart->ram_size)
		cart->ram[offset] = val;
	cart->save_needed = 1;
	pthread_mutex_unlock(&gb->save.lock);
}
