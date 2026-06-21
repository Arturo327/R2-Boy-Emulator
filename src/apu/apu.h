#ifndef APU_H
#define APU_H

#include <stdint.h>

#define APU_BUFFER_LEN 4096

typedef struct APU {

	uint8_t enabled;

	uint8_t nr10, nr11, nr12, nr13, nr14;
	uint8_t nr21, nr22, nr23, nr24;
	uint8_t nr30, nr31, nr32, nr33, nr34;
	uint8_t nr41, nr42, nr43, nr44;
	uint8_t nr50, nr51;

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

#endif
