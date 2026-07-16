#ifndef COMMON_H
#define COMMON_H

#include "gb.h"
#include "cpu/opcodes/opcodes.h"

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

#define DECODE8(fn)		\
void decode_##fn (GB *gb) {	\
	push_mcycle(gb, fn);	\
}

void ld_hl_z (GB *gb);
void ld_z_hl (GB *gb);
void ld_z_imm8 (GB *gb);
void ld_w_imm8 (GB *gb);
void ld_sp_low_imm8 (GB *gb);
void ld_sp_high_imm8 (GB *gb);
void ld_wzmem_sp_low (GB *gb);
void ld_wzmem_sp_high (GB *gb);
void pop_l_z (GB *gb);
void pop_h_z (GB *gb);
void ret (GB *gb);
void check_ret_z (GB *gb);
void check_ret_c (GB *gb);
void check_ret_nz (GB *gb);
void check_ret_nc (GB *gb);
void check_jp_z (GB *gb);
void check_jp_c (GB *gb);
void check_jp_nz (GB *gb);
void check_jp_nc (GB *gb);
void jump (GB *gb);
void push_pc_l (GB *gb);
void push_pc_h (GB *gb);

void inc16_hl (GB *gb);
void inc16_sp (GB *gb);
void inc16_bc (GB *gb);
void inc16_de (GB *gb);
void dec16_hl (GB *gb);
void dec16_sp (GB *gb);
void dec16_bc (GB *gb);
void dec16_de (GB *gb);

void nop (GB *gb);

static inline uint8_t read8 (GB *gb, uint16_t addr) {
	oam_bug(gb, addr, 0);
	return gb->bus.read8(gb->bus.ctx, addr);
}

static inline void write8 (GB *gb, uint16_t addr, uint8_t val) {
	oam_bug(gb, addr, 1);
	gb->bus.write8(gb->bus.ctx, addr, val);
}

static inline void set_z (GB *gb, int v) {
	if (v) gb->cpu.f |= FLAG_Z;
	else gb->cpu.f &= ~FLAG_Z;
}

static inline void set_n (GB *gb, int v) {
	if (v) gb->cpu.f |= FLAG_N;
	else gb->cpu.f &= ~FLAG_N;
}

static inline void set_h (GB *gb, int v) {
	if (v) gb->cpu.f |= FLAG_H;
	else gb->cpu.f &= ~FLAG_H;
}

static inline void set_c (GB *gb, int v) {
	if (v) gb->cpu.f |= FLAG_C;
	else gb->cpu.f &= ~FLAG_C;
}

static inline void push_mcycle (GB *gb, opcode_fn fn) {
	gb->cpu.instr_stack[gb->cpu.instr_tail++] = fn;
}

static inline void clear_instrs (GB *gb) {
	gb->cpu.instr_head = gb->cpu.instr_tail = 0;
}

#endif
