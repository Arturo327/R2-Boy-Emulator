#include "apu/apu.h"
#include <string.h>
 
#define APU_CPU_CLOCK 4194304
 
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
 
static void push_sample (APU *apu, int16_t left, int16_t right) {
	if (apu->buffer_pos + 2 > APU_BUFFER_LEN) return;
	apu->buffer[apu->buffer_pos++] = left;
	apu->buffer[apu->buffer_pos++] = right;
}
 
static int16_t mix_channels (APU *apu) {
	(void) apu;
	return 0; // TODO: sumar canal 1 (pulso+sweep), 2 (pulso), 3 (onda) y 4 (ruido)
}
 
void apu_step (APU *apu) {
 
	apu->frame_seq_counter += 4;
	while (apu->frame_seq_counter >= 8192) {
		apu->frame_seq_counter -= 8192;
		apu->frame_seq_step = (apu->frame_seq_step + 1) & 0x07;
		// TODO: longitud (pasos pares), envelope (paso 7), sweep (pasos 2 y 6)
	}
 
	apu->sample_counter += apu->sample_rate << 2;
	while (apu->sample_counter >= APU_CPU_CLOCK) {
		apu->sample_counter -= APU_CPU_CLOCK;
		int16_t s = mix_channels(apu);
		push_sample(apu, s, s);
	}
}
