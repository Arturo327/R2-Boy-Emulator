#include "cpu/opcodes/common.h"

// --------------------- jr imm8 --------------------

static void jr_z (GB *gb) {
	gb->cpu.pc += (int8_t)gb->cpu.z;
}

static void decode_jr_imm8 (GB *gb) {		// 0x18
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, jr_z);
}

// ------------------- jr cond, imm8 ------------------

static void jr_check_z (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t a8 = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) {
		gb->cpu.z = a8;
		return;
	}
	clear_instrs(gb);
}
static void decode_jr_z_imm8 (GB *gb) {	// 0x28
	push_mcycle(gb, jr_check_z);
	push_mcycle(gb, jr_z);
}

static void jr_check_c (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t a8 = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) {
		gb->cpu.z = a8;
		return;
	}
	clear_instrs(gb);
}
static void decode_jr_c_imm8 (GB *gb) {	// 0x38
	push_mcycle(gb, jr_check_c);
	push_mcycle(gb, jr_z);
}

static void jr_check_nz (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t a8 = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) {
		clear_instrs(gb);
		return;
	}
	gb->cpu.z = a8;
}
static void decode_jr_nz_imm8 (GB *gb) {	// 0x20
	push_mcycle(gb, jr_check_nz);
	push_mcycle(gb, jr_z);
}

static void jr_check_nc (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t a8 = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) {
		clear_instrs(gb);
		return;
	}
	gb->cpu.z = a8;
}
static void decode_jr_nc_imm8 (GB *gb) {	// 0x30
	push_mcycle(gb, jr_check_nc);
	push_mcycle(gb, jr_z);
}

// ------------------ RET ------------------------

static void decode_ret (GB *gb) {
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, ret);
}

// ------------------ RET cond ------------------------

static void decode_ret_z (GB *gb) {		// 0xC9
	push_mcycle(gb, check_ret_z);
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, ret);
}

static void decode_ret_c (GB *gb) {		// 0xD8
	push_mcycle(gb, check_ret_c);
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, ret);
}

static void decode_ret_nz (GB *gb) {		// 0xC0
	push_mcycle(gb, check_ret_nz);
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, ret);
}

static void decode_ret_nc (GB *gb) {		// 0xD0
	push_mcycle(gb, check_ret_nc);
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, ret);
}

// ------------------ RETI ------------------------

static void reti (GB *gb) {			// 0xD9
	gb->cpu.pc = gb->cpu.wz;
	gb->cpu.bus->interrupts->IME = 1;
}
static void decode_reti (GB *gb) {
	push_mcycle(gb, pop_l_z);
	push_mcycle(gb, pop_h_z);
	push_mcycle(gb, reti);
}

// ------------------- JP imm16 -----------------------

static void decode_jp_imm16 (GB *gb) {			// 0xC3
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_w_imm8);
	push_mcycle(gb, jump);
}

// ------------------ JP cond, imm16 ------------------------

static void decode_jp_z_imm16 (GB *gb) {		// 0xCA
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_z);
	push_mcycle(gb, jump);
}

static void decode_jp_c_imm16 (GB *gb) {		// 0xDA
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_c);
	push_mcycle(gb, jump);
}

static void decode_jp_nz_imm16 (GB *gb) {		// 0xC2
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_nz);
	push_mcycle(gb, jump);
}

static void decode_jp_nc_imm16 (GB *gb) {		// 0xD2
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_nc);
	push_mcycle(gb, jump);
}

// --------------------- JP hl -----------------------

static void jp_hl (GB *gb) {				// 0xE9
	gb->cpu.pc = gb->cpu.hl;
}

// --------------------- CALL imm16 ------------------

static void call_wz (GB *gb) {
	push_pc_l(gb);
	jump(gb);
}

static void decode_call_imm16 (GB *gb) {		// 0xCD
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_w_imm8);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, call_wz);
}

// --------------------- CALL cond, imm16 ------------------

static void decode_call_z_imm16 (GB *gb) {		// 0xCC
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_z);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, call_wz);
}

static void decode_call_c_imm16 (GB *gb) {		// 0xDC
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_c);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, call_wz);
}

static void decode_call_nz_imm16 (GB *gb) {		// 0xC4
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_nz);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, call_wz);
}

static void decode_call_nc_imm16 (GB *gb) {		// 0xD4
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, check_jp_nc);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, call_wz);
}

// -------------------- RST tgt3 -------------------------

#define DEF_RST(num)			\
static void rst_##num (GB *gb) {	\
	push_pc_l(gb);			\
	gb->cpu.pc = num << 3;		\
}

DEF_RST(0)	// 0xC7
DEF_RST(1)	// 0xCF
DEF_RST(2)	// 0xD7
DEF_RST(3)	// 0xDF
DEF_RST(4)	// 0xE7
DEF_RST(5)	// 0xEF
DEF_RST(6)	// 0xF7
DEF_RST(7)	// 0xFF

#undef DEF_RST

#define DEF_RST(num)			\
static void decode_rst_##num (GB *gb) {	\
	push_mcycle(gb, dec16_sp);	\
	push_mcycle(gb, push_pc_h);	\
	push_mcycle(gb, rst_##num);	\
}

DEF_RST(0)	// 0xC7
DEF_RST(1)	// 0xCF
DEF_RST(2)	// 0xD7
DEF_RST(3)	// 0xDF
DEF_RST(4)	// 0xE7
DEF_RST(5)	// 0xEF
DEF_RST(6)	// 0xF7
DEF_RST(7)	// 0xFF

#undef DEF_RST

void init_jump (OpcodeTable *t)
{
	t->main[0x18] = decode_jr_imm8;
	t->main[0x20] = decode_jr_nz_imm8;
	t->main[0x28] = decode_jr_z_imm8;
	t->main[0x30] = decode_jr_nc_imm8;
	t->main[0x38] = decode_jr_c_imm8;
	t->main[0xC2] = decode_jp_nz_imm16;
	t->main[0xC3] = decode_jp_imm16;
	t->main[0xC4] = decode_call_nz_imm16;
	t->main[0xC7] = decode_rst_0;
	t->main[0xC8] = decode_ret_z;
	t->main[0xC9] = decode_ret;
	t->main[0xCA] = decode_jp_z_imm16;
	t->main[0xCC] = decode_call_z_imm16;
	t->main[0xCD] = decode_call_imm16;
	t->main[0xCF] = decode_rst_1;
	t->main[0xD0] = decode_ret_nc;
	t->main[0xD2] = decode_jp_nc_imm16;
	t->main[0xD4] = decode_call_nc_imm16;
	t->main[0xD7] = decode_rst_2;
	t->main[0xD8] = decode_ret_c;
	t->main[0xD9] = decode_reti;
	t->main[0xDA] = decode_jp_c_imm16;
	t->main[0xDC] = decode_call_c_imm16;
	t->main[0xDF] = decode_rst_3;
	t->main[0xE7] = decode_rst_4;
	t->main[0xEF] = decode_rst_5;
	t->main[0xE9] = jp_hl;
	t->main[0xF7] = decode_rst_6;
	t->main[0xFF] = decode_rst_7;
	t->main[0xC0] = decode_ret_nz;
}
