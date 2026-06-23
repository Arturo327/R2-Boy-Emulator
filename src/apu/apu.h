#ifndef APU_H
#define APU_H

#include <stdint.h>

#define APU_BUFFER_LEN 4096

typedef struct AnalogChannels {
	int16_t ch1;
	int16_t ch2;
	int16_t ch3;
	int16_t ch4;
} AnalogChannels;

typedef struct Sample {
	int16_t right;
	int16_t left;
} Sample;

typedef struct CH {
	uint16_t freq_timer;
	uint8_t duty_step;
	uint8_t length_counter;
	uint8_t envelope_timer;
	uint8_t volume;
	uint8_t enabled;
} CH;

typedef struct CH1 {
	CH ch;
	uint16_t shadow_freq;
	uint8_t sweep_timer;
	uint8_t sweep_enabled;
	uint8_t negate_used;
} CH1;

typedef struct CH3 {
	uint16_t freq_timer;
	uint16_t length_counter;
	uint8_t enabled;
	uint8_t position;
	uint8_t sample_buffer;
} CH3;

typedef struct CH4 {
	CH ch;
	uint16_t lfsr;
} CH4;

typedef struct APU {

	uint8_t enabled;

	uint8_t nr10, nr11, nr12, nr13, nr14;
	uint8_t nr21, nr22, nr23, nr24;
	uint8_t nr30, nr31, nr32, nr33, nr34;
	uint8_t nr41, nr42, nr43, nr44;
	uint8_t nr50, nr51;

	CH1 ch1;
	CH ch2;
	CH3 ch3;
	CH4 ch4;

	int32_t right_capacitor;
	int32_t left_capacitor;

	uint8_t wave_ram[16];

	uint16_t frame_seq_counter;
	uint8_t frame_seq_step;

	uint32_t sample_rate;
	uint32_t sample_counter;

	int16_t buffer[APU_BUFFER_LEN];
	int buffer_pos;

} APU;

void init_apu (APU *apu);
void init_apu_reg (APU *apu);

void apu_step (APU *apu);
void apu_div_reset (APU *apu, uint8_t old_div);

uint8_t apu_read_reg (APU *apu, uint16_t addr);
void apu_write_reg (APU *apu, uint16_t addr, uint8_t val);

#endif
