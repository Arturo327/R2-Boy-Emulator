#include "cpu/opcodes/common.h"

void ld_hl_z (GB *gb) {
	write8(gb, gb->cpu.hl, gb->cpu.z);
}

void ld_z_hl (GB *gb) {
	gb->cpu.z = read8(gb, gb->cpu.hl);
}

void ld_z_imm8 (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.z = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
}

void ld_w_imm8 (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.w = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
}

void ld_sp_low_imm8 (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.sp = (uint16_t)gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
}

void ld_sp_high_imm8 (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.sp |= ((uint16_t)gb->bus.read8(gb->bus.ctx, gb->cpu.pc++)) << 8;
}

void ld_wzmem_sp_low (GB *gb) {
	uint8_t sp_l = (uint8_t)gb->cpu.sp;
	write8(gb, gb->cpu.wz, sp_l);
}

void ld_wzmem_sp_high (GB *gb) {
	uint8_t sp_h = (uint8_t)(gb->cpu.sp >> 8);
	write8(gb, gb->cpu.wz + 1, sp_h);
}

void pop_l_z (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 2);
	gb->cpu.z = gb->cpu.bus->read8(gb->bus.ctx, gb->cpu.sp++);
}
void pop_h_z (GB *gb) {
	gb->cpu.w = read8(gb, gb->cpu.sp++);
}
void ret (GB *gb) {
	gb->cpu.pc = gb->cpu.wz;
}

void check_ret_z (GB *gb) {
	if (!(gb->cpu.f & FLAG_Z)) clear_instrs(gb);
}
void check_ret_c (GB *gb) {
	if (!(gb->cpu.f & FLAG_C)) clear_instrs(gb);
}
void check_ret_nz (GB *gb) {
	if (gb->cpu.f & FLAG_Z) clear_instrs(gb);
}
void check_ret_nc (GB *gb) {
	if (gb->cpu.f & FLAG_C) clear_instrs(gb);
}

void check_jp_z (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.w = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (!(gb->cpu.f & FLAG_Z)) clear_instrs(gb);
}
void check_jp_c (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.w = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (!(gb->cpu.f & FLAG_C)) clear_instrs(gb);
}
void check_jp_nz (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.w = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) clear_instrs(gb);
}
void check_jp_nc (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.w = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) clear_instrs(gb);
}

void jump (GB *gb) {
	gb->cpu.pc = gb->cpu.wz;
}

void push_pc_h (GB *gb) {
	write8(gb, gb->cpu.sp, (uint8_t)(gb->cpu.pc >> 8));
}
void push_pc_l (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 1);
	gb->bus.write8(gb->bus.ctx, --gb->cpu.sp, (uint8_t)gb->cpu.pc);
}

void nop (GB *gb) {
	(void)gb;
}

#define DEF_INC(name)			\
void inc16_##name (GB *gb) {		\
	oam_bug(gb, gb->cpu.name, 1);	\
	gb->cpu.name++;			\
}

DEF_INC(bc)		// 0x03
DEF_INC(de)		// 0x13
DEF_INC(hl)		// 0x23
DEF_INC(sp)		// 0x33

#undef DEF_INC

#define DEF_DEC16(name)			\
void dec16_##name (GB *gb) {		\
	oam_bug(gb, gb->cpu.name, 1);	\
	gb->cpu.name--;			\
}

DEF_DEC16(bc)		// 0x0B
DEF_DEC16(de)		// 0x1B
DEF_DEC16(hl)		// 0x2B
DEF_DEC16(sp)		// 0x3B

#undef DEF_DEC16
