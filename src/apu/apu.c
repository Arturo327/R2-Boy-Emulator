#include "apu/apu.h"
#include <string.h>
 
#define APU_CPU_CLOCK 4194304
#define MASTER_GAIN 32

static const uint8_t DUTY_TABLE[4] = { 0x01, 0x81, 0x87, 0x7E };
 
void init_apu (APU *apu) {
	memset(apu, 0, sizeof(APU));
	apu->sample_rate = 44100;
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
 
static void push_sample (APU *apu, Sample s) {
	if (apu->buffer_pos + 2 > APU_BUFFER_LEN) return;
	apu->buffer[apu->buffer_pos++] = s.left;
	apu->buffer[apu->buffer_pos++] = s.right;
}

static void clock_length (CH *ch, uint8_t nr_x4) {
	if ((nr_x4 & 0x40) && ch->length_counter > 0)
		if (--ch->length_counter == 0) ch->enabled = 0;
}
static void clock_envelope (CH *ch, uint8_t nr_x2) {
	uint8_t period = nr_x2 & 0x07;
	if (!period) return;
	if (ch->envelope_timer > 0) ch->envelope_timer--;
	if (ch->envelope_timer == 0) {
		ch->envelope_timer = period;
		if ((nr_x2 & 0x08) && ch->volume < 15) ch->volume++;
		else if (!(nr_x2 & 0x08) && ch->volume > 0) ch->volume--;
	}
}

static uint16_t calc_sweep (APU *apu, int *overflow) {
	uint8_t shift = apu->nr10 & 0x07;
	uint16_t delta = apu->ch1.shadow_freq >> shift;
	uint16_t new_freq = (apu->nr10 & 0x08)
		? apu->ch1.shadow_freq - delta
		: apu->ch1.shadow_freq + delta;

	*overflow = (new_freq > 2047);
	return new_freq;
}

static void clock_sweep (APU *apu) {
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

static void trigger_pulse_common (CH *ch, uint8_t nr_x1, uint8_t nr_x2,
		uint8_t nr_x3, uint8_t nr_x4) {
	ch->enabled = (nr_x2 & 0xF8) != 0;
	if (ch->length_counter == 0)
		ch->length_counter = 64 - (nr_x1 & 0x3F);

	uint16_t freq = nr_x3 | ((nr_x4 & 0x07) << 8);
	ch->freq_timer = (2048 - freq) << 2;
	ch->envelope_timer = (nr_x2 & 0x07) ? (nr_x2 & 0x07) : 8;
	ch->volume = nr_x2 >> 4;
}

void apu_trigger_ch1 (APU *apu) {
	trigger_pulse_common(&apu->ch1.ch, apu->nr11, apu->nr12, apu->nr13, apu->nr14);

	uint16_t freq = apu->nr13 | ((apu->nr14 & 0x07) << 8);
	apu->ch1.shadow_freq  = freq;
	uint8_t period = (apu->nr10 & 0x70) >> 4;
	apu->ch1.sweep_timer   = period ? period : 8;
	apu->ch1.sweep_enabled = period || (apu->nr10 & 0x07);

	if (apu->nr10 & 0x07) {
		int overflow;
		calc_sweep(apu, &overflow);
		if (overflow) apu->ch1.ch.enabled = 0;
	}
}

void apu_trigger_ch2 (APU *apu) {
	trigger_pulse_common(&apu->ch2, apu->nr21, apu->nr22, apu->nr23, apu->nr24);
}

static int16_t dac (CH *ch, uint8_t nr_x1) {
	if (!ch->enabled) return 0;
	uint8_t bit = (DUTY_TABLE[nr_x1 >> 6] >> ch->duty_step) & 1;
	return bit ? (int16_t)ch->volume : -(int16_t)ch->volume;
}
static void step_freq (CH *ch, uint8_t nr_x3, uint8_t nr_x4) {
	if (ch->freq_timer <= 4) {
		uint16_t freq = nr_x3 | ((nr_x4 & 0x07) << 8);
		ch->freq_timer += (2048 - freq) * 4;
		ch->duty_step = (ch->duty_step + 1) & 7;
	}
	ch->freq_timer -= 4;
}

static int16_t dac_ch3 (APU *apu) {
	return 0;
}

static int16_t dac_ch4 (APU *apu) {
	return 0;
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
 
void apu_step (APU *apu) {

	step_freq(&apu->ch1.ch, apu->nr13, apu->nr14);
	step_freq(&apu->ch2, apu->nr23, apu->nr24);
 
	apu->frame_seq_counter += 4;
	if (apu->frame_seq_counter >= 8192) {
		apu->frame_seq_counter -= 8192;
		apu->frame_seq_step = (apu->frame_seq_step + 1) & 7;

		if ((apu->frame_seq_step & 1) == 0) {
			clock_length(&apu->ch1.ch, apu->nr14);
			clock_length(&apu->ch2, apu->nr24);
		}
		if (apu->frame_seq_step == 2 || apu->frame_seq_step == 6) clock_sweep(apu);
		if (apu->frame_seq_step == 7) {
			clock_envelope(&apu->ch1.ch, apu->nr12);
			clock_envelope(&apu->ch2, apu->nr22);
		}
	}
 
	apu->sample_counter += apu->sample_rate << 2;
	if (apu->sample_counter >= APU_CPU_CLOCK) {
		apu->sample_counter -= APU_CPU_CLOCK;
		Sample s = mix_channels(apu);
		push_sample(apu, s);
	}
}
















