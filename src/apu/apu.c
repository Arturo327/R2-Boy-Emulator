#include "apu/apu.h"
#include <string.h>
 
#define APU_CPU_CLOCK 4194304
#define MASTER_GAIN 32

static const uint8_t DUTY_TABLE[4] = { 0x01, 0x81, 0x87, 0x7E };
static const uint16_t NOISE_DIVISOR[8] = {8,16,32,48,64,80,96,112};
static const uint8_t WAVE_SHIFT[4] = {4, 0, 1, 2};
 
void init_apu (APU *apu) {
	memset(apu, 0, sizeof(APU));
	apu->sample_rate = 44100;
	apu->ch1.negate_used = 0;
}
 
void init_apu_reg (APU *apu) {
	apu->nr10 = 0x80;
	apu->nr11 = 0xBF;
	apu->nr12 = 0xF3;
	apu->nr13 = 0xFF;
	apu->nr14 = 0xBF;
 
	apu->nr21 = 0x3F;
	apu->nr22 = 0x00;
	apu->nr23 = 0xFF;
	apu->nr24 = 0xBF;
 
	apu->nr30 = 0x7F;
	apu->nr31 = 0xFF;
	apu->nr32 = 0x9F;
	apu->nr33 = 0xFF;
	apu->nr34 = 0xBF;
 
	apu->nr41 = 0xFF;
	apu->nr42 = 0x00;
	apu->nr43 = 0x00;
	apu->nr44 = 0xBF;
 
	apu->nr50 = 0x77;
	apu->nr51 = 0xF3;
 
	apu->enabled = 1;
}

static void apu_power_off (APU *apu)
{
	uint8_t len1 = apu->ch1.ch.length_counter;
	uint8_t len2 = apu->ch2.length_counter;
	uint8_t len3 = apu->ch3.length_counter;
	uint8_t len4 = apu->ch4.ch.length_counter;

	memset(&apu->ch1, 0, sizeof(apu->ch1));
	memset(&apu->ch2, 0, sizeof(apu->ch2));
	memset(&apu->ch3, 0, sizeof(apu->ch3));
	memset(&apu->ch4, 0, sizeof(apu->ch4));

	apu->ch1.ch.length_counter = len1;
	apu->ch2.length_counter = len2;
	apu->ch3.length_counter = len3;
	apu->ch4.ch.length_counter = len4;

	apu->nr10 = apu->nr11 = apu->nr12 = apu->nr13 = apu->nr14 = 0;
	apu->nr21 = apu->nr22 = apu->nr23 = apu->nr24 = 0;
	apu->nr30 = apu->nr31 = apu->nr32 = apu->nr33 = apu->nr34 = 0;
	apu->nr41 = apu->nr42 = apu->nr43 = apu->nr44 = 0;
	apu->nr50 = apu->nr51 = 0;
	apu->enabled = 0;
}
 
static void push_sample (APU *apu, Sample s)
{
	if (apu->buffer_pos + 2 > APU_BUFFER_LEN) return;
	apu->buffer[apu->buffer_pos++] = s.left;
	apu->buffer[apu->buffer_pos++] = s.right;
}

static void clock_length (CH *ch, uint8_t nr_x4)
{
	if ((nr_x4 & 0x40) && ch->length_counter > 0)
		if (--ch->length_counter == 0) ch->enabled = 0;
}
static void clock_length_ch3 (APU *apu)
{
	if ((apu->nr34 & 0x40) && apu->ch3.length_counter > 0)
		if (--apu->ch3.length_counter == 0) apu->ch3.enabled = 0;
}

static void clock_envelope (CH *ch, uint8_t nr_x2)
{
	uint8_t period = nr_x2 & 0x07;
	if (!period) return;
	if (ch->envelope_timer > 0) ch->envelope_timer--;
	if (ch->envelope_timer == 0) {
		ch->envelope_timer = period;
		if ((nr_x2 & 0x08) && ch->volume < 15) ch->volume++;
		else if (!(nr_x2 & 0x08) && ch->volume > 0) ch->volume--;
	}
}

static uint16_t calc_sweep (APU *apu, int *overflow)
{
	uint8_t shift = apu->nr10 & 0x07;
	uint16_t delta = apu->ch1.shadow_freq >> shift;
	uint16_t new_freq;

	if (apu->nr10 & 0x08) {
		apu->ch1.negate_used = 1;
		new_freq = apu->ch1.shadow_freq - delta;
	} else {
		new_freq = apu->ch1.shadow_freq + delta;
	}

	*overflow = (new_freq > 2047);
	return new_freq;
}

static void clock_sweep (APU *apu)
{
	if (apu->ch1.sweep_timer > 0) apu->ch1.sweep_timer--;
	if (apu->ch1.sweep_timer != 0) return;

	uint8_t period = (apu->nr10 & 0x70) >> 4;
	apu->ch1.sweep_timer = period ? period : 8;
	if (!apu->ch1.sweep_enabled || period == 0) return;

	int overflow;
	uint16_t new_freq = calc_sweep(apu, &overflow);

	if (overflow) {
		apu->ch1.ch.enabled = 0;
		return;
	}

	if ((apu->nr10 & 0x07) != 0) {
		apu->ch1.shadow_freq = new_freq;
		apu->nr13 = new_freq & 0xFF;
		apu->nr14 = (apu->nr14 & 0xF8) | (new_freq >> 8);

		calc_sweep(apu, &overflow);
		if (overflow) apu->ch1.ch.enabled = 0;
	}
}

static void step_wave (APU *apu)
{
	CH3 *ch3 = &apu->ch3;
	if (ch3->freq_timer <= 4) {
		uint16_t freq = apu->nr33 | ((apu->nr34 & 0x07) << 8);
		ch3->freq_timer += (2048 - freq) * 2;
		ch3->position = (ch3->position + 1) & 31;

		uint8_t byte = apu->wave_ram[apu->ch3.position >> 1];
		ch3->sample_buffer = (ch3->position & 1) ? (byte & 0x0F) : (byte >> 4);
	}
	ch3->freq_timer -= 4;
}

static void step_noise (CH4 *ch4, uint8_t nr43)
{
	if (ch4->ch.freq_timer <= 4) {
		uint8_t shift = nr43 >> 4;
		uint8_t code = nr43 & 0x07;
		ch4->ch.freq_timer += (uint16_t)(NOISE_DIVISOR[code] << shift);

		uint16_t lfsr = ch4->lfsr;
		uint8_t xor_bit = (lfsr ^ (lfsr >> 1)) & 1;
		lfsr >>= 1;
		lfsr |= xor_bit << 14;
		if (nr43 & 0x08)
			lfsr = (lfsr & ~(1 << 6)) | (xor_bit << 6);
		
		ch4->lfsr = lfsr;
	}
	ch4->ch.freq_timer -= 4;
}

static void trigger_common (APU *apu, CH *ch, uint8_t nr_x2, uint8_t nr_x4)
{
	ch->enabled = (nr_x2 & 0xF8) != 0;
	if (ch->length_counter == 0) {
		ch->length_counter = 64;
		if ((nr_x4 & 0x40) && !(apu->frame_seq_step & 1))
			ch->length_counter--;
	}
	ch->envelope_timer = (nr_x2 & 0x07) ? (nr_x2 & 0x07) : 8;
	ch->volume = nr_x2 >> 4;
}

static void trigger_pulse_common (APU *apu, CH *ch, uint8_t nr_x2,
		uint8_t nr_x3, uint8_t nr_x4)
{
	trigger_common (apu, ch, nr_x2, nr_x4);
	uint16_t freq = nr_x3 | ((nr_x4 & 0x07) << 8);
	ch->freq_timer = (2048 - freq) << 2;
}

void apu_trigger_ch1 (APU *apu)
{
	trigger_pulse_common(apu, &apu->ch1.ch, apu->nr12, apu->nr13, apu->nr14);

	uint16_t freq = apu->nr13 | ((apu->nr14 & 0x07) << 8);
	apu->ch1.shadow_freq = freq;
	uint8_t period = (apu->nr10 & 0x70) >> 4;
	apu->ch1.sweep_timer = period ? period : 8;
	apu->ch1.sweep_enabled = period || (apu->nr10 & 0x07);
	apu->ch1.negate_used = 0;
	
	if (apu->nr10 & 0x07) {
		int overflow;
		calc_sweep(apu, &overflow);
		if (overflow) apu->ch1.ch.enabled = 0;
	}
}

void apu_trigger_ch2 (APU *apu)
{
	trigger_pulse_common(apu, &apu->ch2, apu->nr22, apu->nr23, apu->nr24);
}

void apu_trigger_ch3 (APU *apu)
{
	apu->ch3.enabled = (apu->nr30 & 0x80) != 0;

	if (apu->ch3.length_counter == 0) {
		apu->ch3.length_counter = 256;
		if ((apu->nr34 & 0x40) && !(apu->frame_seq_step & 1))
			apu->ch3.length_counter--;
	}

	uint16_t freq = apu->nr33 | ((apu->nr34 & 0x07) << 8);
	apu->ch3.freq_timer = (2048 - freq) * 2;
	apu->ch3.position = 0;
}

void apu_trigger_ch4 (APU *apu)
{
	trigger_common(apu, &apu->ch4.ch, apu->nr42, apu->nr44);

	apu->ch4.lfsr = 0x7FFF;
	uint8_t shift = apu->nr43 >> 4;
	uint8_t code = apu->nr43 & 0x07;
	apu->ch4.ch.freq_timer = NOISE_DIVISOR[code] << shift;
}

static int16_t dac (CH *ch, uint8_t nr_x1)
{
	if (!ch->enabled) return 0;
	uint8_t bit = (DUTY_TABLE[nr_x1 >> 6] >> ch->duty_step) & 1;
	return bit ? (int16_t)ch->volume : -(int16_t)ch->volume;
}

static void step_freq (CH *ch, uint8_t nr_x3, uint8_t nr_x4)
{
	if (ch->freq_timer <= 4) {
		uint16_t freq = nr_x3 | ((nr_x4 & 0x07) << 8);
		ch->freq_timer += (2048 - freq) * 4;
		ch->duty_step = (ch->duty_step + 1) & 7;
	}
	ch->freq_timer -= 4;
}

static int16_t dac_ch3 (APU *apu)
{
	if (!apu->ch3.enabled) return 0;
	uint8_t shift = WAVE_SHIFT[(apu->nr32 >> 5) & 0x03];
	uint8_t sample = apu->ch3.sample_buffer >> shift;
	return (int16_t)sample - 8;
}

static int16_t dac_ch4 (APU *apu)
{
	CH *ch = &apu->ch4.ch;
	if (!ch->enabled) return 0;
	uint8_t bit = ~(apu->ch4.lfsr) & 1;
	return bit ? (int16_t)ch->volume : -(int16_t)ch->volume;
}

static Sample mixer (APU *apu, AnalogChannels chs) {
	Sample s;

	s.right = (apu->nr51 & 0x01) ? chs.ch1 : 0;
	s.left = (apu->nr51 & 0x10) ? chs.ch1 : 0;
	
	s.right += (apu->nr51 & 0x02) ? chs.ch2 : 0;
	s.left += (apu->nr51 & 0x20) ? chs.ch2 : 0;
	
	s.right += (apu->nr51 & 0x04) ? chs.ch3 : 0;
	s.left += (apu->nr51 & 0x40) ? chs.ch3 : 0;
	
	s.right += (apu->nr51 & 0x08) ? chs.ch4 : 0;
	s.left += (apu->nr51 & 0x80) ? chs.ch4 : 0;
	
	return s;
}

static void volume (APU *apu, Sample *s) {
	s->right = s->right * ((apu->nr50 & 0x07) + 1) * MASTER_GAIN;
	s->left = s->left * (((apu->nr50 & 0x70) >> 4) + 1) * MASTER_GAIN;
}

static void HPF (APU *apu, Sample *s) {
	int32_t r = s->right - apu->right_capacitor;
	apu->right_capacitor += r >> 10;
	s->right = (int16_t) r;

	int32_t l = s->left - apu->left_capacitor;
	apu->left_capacitor += l >> 10;
	s->left = (int16_t) l;
}

static Sample mix_channels (APU *apu) {

	AnalogChannels chs;
	chs.ch1 = dac(&apu->ch1.ch, apu->nr11);
	chs.ch2 = dac(&apu->ch2, apu->nr21);
	chs.ch3 = dac_ch3(apu);
	chs.ch4 = dac_ch4(apu);
	
	Sample s = mixer(apu, chs);
	volume(apu, &s);
	HPF(apu, &s);
	return s;
}

static void clock_frame_seq (APU *apu)
{
	apu->frame_seq_step = (apu->frame_seq_step + 1) & 7;

	if ((apu->frame_seq_step & 1) == 0) {
		clock_length(&apu->ch1.ch, apu->nr14);
		clock_length(&apu->ch2, apu->nr24);
		clock_length_ch3(apu);
		clock_length(&apu->ch4.ch, apu->nr44);
	}
	if (apu->frame_seq_step == 2 || apu->frame_seq_step == 6) clock_sweep(apu);
	if (apu->frame_seq_step == 7) {
		clock_envelope(&apu->ch1.ch, apu->nr12);
		clock_envelope(&apu->ch2, apu->nr22);
		clock_envelope(&apu->ch4.ch, apu->nr42);
	}
}
 
void apu_step (APU *apu) 
{
	step_freq(&apu->ch1.ch, apu->nr13, apu->nr14);
	step_freq(&apu->ch2, apu->nr23, apu->nr24);
	step_wave(apu);
	step_noise(&apu->ch4, apu->nr43);
 
	apu->frame_seq_counter += 4;
	if (apu->frame_seq_counter >= 8192) {
		apu->frame_seq_counter -= 8192;
		clock_frame_seq(apu);
	}
 
	apu->sample_counter += apu->sample_rate << 2;
	if (apu->sample_counter >= APU_CPU_CLOCK) {
		apu->sample_counter -= APU_CPU_CLOCK;
		Sample s = mix_channels(apu);
		push_sample(apu, s);
	}
}

void apu_div_reset (APU *apu, uint8_t old_div)
{
	if (old_div & 0x10)
		clock_frame_seq(apu);
	apu->frame_seq_counter = 0;
}

uint8_t apu_read_reg (APU *apu, uint16_t addr) {

	if (addr >= 0xFF30 && addr <= 0xFF3F)
		return apu->wave_ram[addr - 0xFF30];

	switch (addr) {
		case 0xFF10: return apu->nr10 | 0x80;
		case 0xFF11: return apu->nr11 | 0x3F;
		case 0xFF12: return apu->nr12;
		case 0xFF13: return 0xFF;
		case 0xFF14: return apu->nr14 | 0xBF;

		case 0xFF16: return apu->nr21 | 0x3F;
		case 0xFF17: return apu->nr22;
		case 0xFF18: return 0xFF;
		case 0xFF19: return apu->nr24 | 0xBF;

		case 0xFF1A: return apu->nr30 | 0x7F;
		case 0xFF1B: return 0xFF;
		case 0xFF1C: return apu->nr32 | 0x9F;
		case 0xFF1D: return 0xFF;
		case 0xFF1E: return apu->nr34 | 0xBF;

		case 0xFF20: return 0xFF;
		case 0xFF21: return apu->nr42;
		case 0xFF22: return apu->nr43;
		case 0xFF23: return apu->nr44 | 0xBF;

		case 0xFF24: return apu->nr50;
		case 0xFF25: return apu->nr51;
		case 0xFF26: {
			uint8_t status = 0;
			if (apu->ch1.ch.enabled) status |= 0x01;
			if (apu->ch2.enabled) status |= 0x02;
			if (apu->ch3.enabled) status |= 0x04;
			if (apu->ch4.ch.enabled) status |= 0x08;
			return (apu->enabled ? 0x80 : 0x00) | 0x70 | status;
		}

		default: return 0xFF;
	}
}

void apu_write_reg (APU *apu, uint16_t addr, uint8_t val) {
	
	if (addr >= 0xFF30 && addr <= 0xFF3F) {
		apu->wave_ram[addr - 0xFF30] = val;
		return;
	}

	if (!apu->enabled) {
		switch (addr) {
			case 0xFF11: apu->ch1.ch.length_counter = 64 - (val & 0x3F); break;
			case 0xFF16: apu->ch2.length_counter = 64 - (val & 0x3F); break;
			case 0xFF1B: apu->ch3.length_counter = 256 - val; break;
			case 0xFF20: apu->ch4.ch.length_counter = 64 - (val & 0x3F); break;
			case 0xFF26: {
				if (val & 0x80) {
					apu->enabled = 1;
					apu->frame_seq_step = 0;
				}
				break;
			}
			default: break;
		}
		return;
	}

	switch (addr) {
		case 0xFF10: {
			uint8_t prev = apu->nr10;
			apu->nr10 = val;
			if ((prev & 0x08) && !(val & 0x08) && apu->ch1.negate_used)
				apu->ch1.ch.enabled = 0;
			break;
		}
		case 0xFF11: apu->nr11 = val; apu->ch1.ch.length_counter = 64 - (val & 0x3F); break;
		case 0xFF12: apu->nr12 = val; if (!(val & 0xF8)) apu->ch1.ch.enabled = 0; break;
		case 0xFF13: apu->nr13 = val; break;
		case 0xFF14: {
			uint8_t prev = apu->nr14;
			apu->nr14 = val & 0xC7;
			
			if (!(prev & 0x40) && (val & 0x40) && !(apu->frame_seq_step & 1))
				clock_length(&apu->ch1.ch, apu->nr14);

			if (val & 0x80) apu_trigger_ch1(apu);
			break;
		}

		case 0xFF16: apu->nr21 = val; apu->ch2.length_counter = 64 - (val & 0x3F); break;
		case 0xFF17: apu->nr22 = val; if (!(val & 0xF8)) apu->ch2.enabled = 0; break;
		case 0xFF18: apu->nr23 = val; break;
		case 0xFF19: {
			uint8_t prev = apu->nr24;
			apu->nr24 = val & 0xC7;
			
			if (!(prev & 0x40) && (val & 0x40) && !(apu->frame_seq_step & 1))
				clock_length(&apu->ch2, apu->nr24);

			if (val & 0x80) apu_trigger_ch2(apu);
			break;
		}

		case 0xFF1A: apu->nr30 = val; if (!(val & 0x80)) apu->ch3.enabled = 0; break;
		case 0xFF1B: apu->nr31 = val; apu->ch3.length_counter = 256 - val; break;
		case 0xFF1C: apu->nr32 = val; break;
		case 0xFF1D: apu->nr33 = val; break;
		case 0xFF1E: {
			uint8_t prev = apu->nr34;
			apu->nr34 = val & 0xC7;

			if (!(prev & 0x40) && (val & 0x40) && !(apu->frame_seq_step & 1))
				clock_length_ch3(apu);

			if (val & 0x80) apu_trigger_ch3(apu);
				break;
		}

		case 0xFF20: apu->nr41 = val; apu->ch4.ch.length_counter = 64 - (val & 0x3F); break;
		case 0xFF21: apu->nr42 = val; if (!(val & 0xF8)) apu->ch4.ch.enabled = 0; break;
		case 0xFF22: apu->nr43 = val; break;
		case 0xFF23: {
			uint8_t prev = apu->nr44;
			apu->nr44 = val & 0xC7;
			
			if (!(prev & 0x40) && (val & 0x40) && !(apu->frame_seq_step & 1))
				clock_length(&apu->ch4.ch, apu->nr44);

			if (val & 0x80) apu_trigger_ch4(apu);
			break;
		}

		case 0xFF24: apu->nr50 = val; break;
		case 0xFF25: apu->nr51 = val; break;
		case 0xFF26: {
			if (!(val & 0x80)) {
				apu_power_off(apu);
			} else {
				apu->enabled = 1;
			}
			break;
		}
		
	}
}
