#include "cartucho/mbc/mbc7.h"
#include "gb.h"

#define MBC7_CENTER 0x81D0u
#define MBC7_TILT 0x0070u
#define MBC7_GRAVITY 9.80665f

static int16_t combine_axis (uint8_t kb_neg, uint8_t kb_pos, int16_t stick)
{
	if (kb_neg) return -32768;
	if (kb_pos) return 32767;
	return stick;
}

static uint16_t sim_axis_v (int16_t v)
{
	int32_t val = (int32_t)MBC7_CENTER + ((int32_t)MBC7_TILT * v) / 32767;
	if (val < 0) val = 0;
	if (val > 0xFFFF) val = 0xFFFF;
	return (uint16_t)val;
}

static int16_t accel_to_stick (float accel_ms2)
{
	int32_t v = (int32_t)(accel_ms2 * (32767.0f / MBC7_GRAVITY));
	if (v > 32767) v = 32767;
	if (v < -32768) v = -32768;
	return (int16_t)v;
}

static void sample_accelerometer (GB *gb, uint16_t *x, uint16_t *y)
{
	float acel[3];

	if (gb->cfg.use_acel && gamepad_get_acel(&gb->pad, acel)) {
		*x = sim_axis_v(accel_to_stick(acel[0]));
		*y = sim_axis_v(accel_to_stick(-acel[1]));
		return;
	}

	Joypad *j = &gb->joypad;

	int16_t vx = combine_axis(j->kb_tilt & TILT_LEFT, j->kb_tilt & TILT_RIGHT, j->pad_tilt_x);
	int16_t vy = combine_axis(j->kb_tilt & TILT_UP, j->kb_tilt & TILT_DOWN, j->pad_tilt_y);

	*x = sim_axis_v(vx);
	*y = sim_axis_v(vy);
}

static inline uint16_t ee_read (Cartucho *cart, uint8_t idx)
{
	uint16_t lo = cart->ram[idx << 1];
	uint16_t hi = cart->ram[(idx << 1) + 1];
	return (uint16_t)(lo | (hi << 8));
}

static inline void ee_write (Cartucho *cart, uint8_t idx, uint16_t val)
{
	cart->ram[idx << 1] = (uint8_t)(val & 0xFF);
	cart->ram[(idx << 1) + 1] = (uint8_t)(val >> 8);
}

static void eeprom_idle (MBC7State *m)
{
	m->start_seen = 0;
	m->shift_reg = 0;
	m->bit_count = 0;
	m->ee_state = EE_IDLE;
}

static void eeprom_decode (GB *gb)
{
	Cartucho *cart = &gb->memory.cart;
	MBC7State *m = (MBC7State *)cart->state;
	uint16_t instr = m->shift_reg & 0x3FF;
	uint8_t op = (instr >> 8) & 0x03;
	uint8_t addr = instr & 0x7F;

	switch (op)
	{
	case 0x2:
		m->shift_reg = ee_read(cart, addr);
		m->bit_count = 16;
		m->ee_state = EE_READING;
		return;

	case 0x1:
		m->op_addr = addr;
		m->write_all = 0;
		m->shift_reg = 0;
		m->bit_count = 16;
		m->ee_state = EE_WRITING;
		return;

	case 0x3:
		if (m->eeprom_enabled) {
			pthread_mutex_lock(&gb->save.lock);
			ee_write(cart, addr, 0xFFFF);
			cart->save_needed = 1;
			pthread_mutex_unlock(&gb->save.lock);
		}
		eeprom_idle(m);
		return;

	default: {
		uint8_t sub = (instr >> 6) & 0x03;
		switch (sub)
		{
		case 0x3: m->eeprom_enabled = 1; eeprom_idle(m); return;
		case 0x0: m->eeprom_enabled = 0; eeprom_idle(m); return;
		case 0x2:
			if (m->eeprom_enabled) {
				pthread_mutex_lock(&gb->save.lock);
				for (int i = 0; i < 128; i++) ee_write(cart, (uint8_t)i, 0xFFFF);
				cart->save_needed = 1;
				pthread_mutex_unlock(&gb->save.lock);
			}
			eeprom_idle(m);
			return;
		default:
			m->write_all = 1;
			m->shift_reg = 0;
			m->bit_count = 16;
			m->ee_state = EE_WRITING;
			return;
		}
	}
	}
}

static void eeprom_bit_in (GB *gb)
{
	MBC7State *m = (MBC7State *)gb->memory.cart.state;

	if (!m->start_seen) {
		if (m->di) {
			m->start_seen = 1;
			m->shift_reg = 0;
			m->bit_count = 0;
		}
		return;
	}
	m->shift_reg = (uint16_t)((m->shift_reg << 1) | m->di);
	if (++m->bit_count == 10) eeprom_decode(gb);
}

static void eeprom_clock_rise (GB *gb)
{
	Cartucho *cart = &gb->memory.cart;
	MBC7State *m = (MBC7State *)cart->state;

	switch (m->ee_state)
	{
	case EE_READING:
		m->do_bit = (uint8_t)((m->shift_reg >> 15) & 1);
		m->shift_reg = (uint16_t)(m->shift_reg << 1);
		if (--m->bit_count == 0) eeprom_idle(m);
		return;

	case EE_WRITING:
		m->shift_reg = (uint16_t)((m->shift_reg << 1) | m->di);
		if (--m->bit_count == 0) {
			if (m->eeprom_enabled) {
				pthread_mutex_lock(&gb->save.lock);
				if (m->write_all)
					for (int i = 0; i < 128; i++) ee_write(cart, (uint8_t)i, m->shift_reg);
				else
					ee_write(cart, m->op_addr, m->shift_reg);
				cart->save_needed = 1;
				pthread_mutex_unlock(&gb->save.lock);
			}
			m->do_bit = 1;
			eeprom_idle(m);
		}
		return;

	default:
		eeprom_bit_in(gb);
		return;
	}
}

static void eeprom_pin_write (GB *gb, uint8_t val)
{
	MBC7State *m = (MBC7State *)gb->memory.cart.state;
	uint8_t new_cs = (val >> 7) & 1;
	uint8_t new_clk = (val >> 6) & 1;
	uint8_t new_di = (val >> 1) & 1;

	if (!new_cs) {
		m->cs = 0;
		eeprom_idle(m);
		return;
	}

	if (!m->cs) eeprom_idle(m);
	m->cs = 1;
	m->di = new_di;
	if (new_clk && !m->clk) eeprom_clock_rise(gb);
	m->clk = new_clk;
}

static uint8_t eeprom_pin_read (MBC7State *m)
{
	return (uint8_t)((m->cs << 7) | (m->clk << 6) | (m->di << 1) | (m->do_bit & 1));
}

int mbc7_init (Cartucho *cart)
{
	cart->state = malloc(sizeof(MBC7State));
	if (!cart->state) return 0;
	memset(cart->state, 0, sizeof(MBC7State));
	MBC7State *m = (MBC7State *)cart->state;
	m->latch_x = 0x8000;
	m->latch_y = 0x8000;
	return 1;
}

void mbc7_free (Cartucho *cart)
{
	free(cart->state);
	cart->state = NULL;
}

uint8_t mbc7_read_rom (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	uint32_t offset;

	if (addr < 0x4000) {
		offset = addr;
	} else {
		uint32_t bank = cart->rom_bank;
		if (cart->rom_banks) bank &= (cart->rom_banks - 1);
		offset = (bank << 14) + (addr - 0x4000);
	}

	return (offset < cart->rom_size) ? cart->rom[offset] : 0xFF;
}

void mbc7_write_rom (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	MBC7State *m = (MBC7State *)cart->state;

	if (addr < 0x2000) {
		m->ram_enable1 = ((val & 0x0F) == 0x0A);

	} else if (addr < 0x4000) {
		cart->rom_bank = val & 0x7F;

	} else if (addr < 0x6000) {
		m->ram_enable2 = (val == 0x40);
	}
}

static inline uint8_t reg_index (uint16_t addr) {
	return (uint8_t)((addr >> 4) & 0x0F);
}

uint8_t mbc7_read_ram (GB *gb, uint16_t addr)
{
	Cartucho *cart = &gb->memory.cart;
	MBC7State *m = (MBC7State *)cart->state;

	if (!m->ram_enable1 || !m->ram_enable2) return 0xFF;
	if (addr >= 0xB000) return 0xFF;
	if (!cart->ram) return 0xFF;

	switch (reg_index(addr))
	{
	case 0x2: return (uint8_t)(m->latch_x & 0xFF);
	case 0x3: return (uint8_t)(m->latch_x >> 8);
	case 0x4: return (uint8_t)(m->latch_y & 0xFF);
	case 0x5: return (uint8_t)(m->latch_y >> 8);
	case 0x6: return 0x00;
	case 0x7: return 0xFF;
	case 0x8: return eeprom_pin_read(m);
	default: return 0xFF;
	}
}

void mbc7_write_ram (GB *gb, uint16_t addr, uint8_t val)
{
	Cartucho *cart = &gb->memory.cart;
	MBC7State *m = (MBC7State *)cart->state;

	if (!m->ram_enable1 || !m->ram_enable2) return;
	if (addr >= 0xB000) return;
	if (!cart->ram) return;

	switch (reg_index(addr))
	{
	case 0x0:
		if (val == 0x55) { m->latched = 1; m->latch_x = m->latch_y = 0x8000; }
		return;
	case 0x1:
		if (val == 0xAA && m->latched == 1) {
			sample_accelerometer(gb, &m->latch_x, &m->latch_y);
			m->latched = 2;
		}
		return;
	case 0x8:
		eeprom_pin_write(gb, val);
		return;
	default:
		return;
	}
}
