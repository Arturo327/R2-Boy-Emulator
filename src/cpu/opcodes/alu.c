#include "cpu/opcodes/common.h"

#define DEF_DECODE_INC(name)		\
void decode_inc16_##name (GB *gb) {	\
	push_mcycle(gb, inc16_##name);	\
}

DEF_DECODE_INC(bc)
DEF_DECODE_INC(de)
DEF_DECODE_INC(hl)
DEF_DECODE_INC(sp)

#undef DEF_DECODE_INC

#define DEF_DECODE_DEC(name)		\
void decode_dec16_##name (GB *gb) {	\
	push_mcycle(gb, dec16_##name);	\
}

DEF_DECODE_DEC(bc)
DEF_DECODE_DEC(de)
DEF_DECODE_DEC(hl)
DEF_DECODE_DEC(sp)

#undef DEF_DECODE_DEC

// ----------------------------- add hl, r16 ----------------------------------

#define DEF_ADD_HL_R16(name)					\
static void add_hl_##name (GB *gb) {				\
	uint16_t old = gb->cpu.hl;				\
	uint16_t val = gb->cpu.name;				\
	gb->cpu.hl += val;					\
	set_n(gb, 0);						\
	set_h(gb, ((old & 0xFFF) + (val & 0xFFF)) > 0xFFF);	\
	set_c(gb, gb->cpu.hl < old);				\
}

DEF_ADD_HL_R16(bc)	// 0x09
DEF_ADD_HL_R16(de)	// 0x19
DEF_ADD_HL_R16(hl)	// 0x29
DEF_ADD_HL_R16(sp)	// 0x39

#undef DEF_ADD_HL_R16

#define DEF_DECODE_ADD_HL_R16(name)		\
static void decode_add_hl_##name (GB *gb) {	\
	push_mcycle(gb, add_hl_##name);		\
}

DEF_DECODE_ADD_HL_R16(bc)
DEF_DECODE_ADD_HL_R16(de)
DEF_DECODE_ADD_HL_R16(hl)
DEF_DECODE_ADD_HL_R16(sp)

#undef DEF_DECODE_ADD_HL_R16

// -------------------------- inc r8 --------------------------

#define DEF_INC(name)			\
static void inc8_##name (GB *gb) {	\
	uint8_t old = gb->cpu.name;	\
	gb->cpu.name++;			\
	set_z(gb, gb->cpu.name == 0);	\
	set_n(gb, 0);			\
	set_h(gb, (old & 0xF) == 0xF);	\
}

DEF_INC(b)		// 0x04
DEF_INC(c)		// 0x0C
DEF_INC(d)		// 0x14
DEF_INC(e)		// 0x1C
DEF_INC(h)		// 0x24
DEF_INC(l)		// 0x2C
DEF_INC(a)		// 0x3C

#undef DEF_INC

static void inc8_hl (GB *gb) {		// 0x34
	uint8_t hl = read8(gb, gb->cpu.hl);
	gb->cpu.z = hl + 1;
	set_z(gb, gb->cpu.z == 0);
	set_n(gb, 0);
	set_h(gb, (hl & 0xF) == 0xF);
}

static void decode_inc8_hl (GB *gb) {
	push_mcycle(gb, inc8_hl);
	push_mcycle(gb, ld_hl_z);
}

// ------------------------ dec r8 --------------------------

#define DEF_DEC(name)			\
static void dec8_##name (GB *gb) {	\
	uint8_t old = gb->cpu.name;	\
	gb->cpu.name--;			\
	set_z(gb, gb->cpu.name == 0);	\
	set_n(gb, 1);			\
	set_h(gb, (old & 0xF) == 0);	\
}

DEF_DEC(b)		// 0x05
DEF_DEC(c)		// 0x0D
DEF_DEC(d)		// 0x15
DEF_DEC(e)		// 0x1D
DEF_DEC(h)		// 0x25
DEF_DEC(l)		// 0x2D
DEF_DEC(a)		// 0x3D

#undef DEF_DEC

static void dec8_hl (GB *gb) {		// 0x35
	uint8_t hl = read8(gb, gb->cpu.hl);
	gb->cpu.z = hl - 1;
	set_z(gb, gb->cpu.z == 0);
	set_n(gb, 1);
	set_h(gb, (hl & 0xF) == 0);
}

static void decode_dec8_hl (GB *gb) {
	push_mcycle(gb, dec8_hl);
	push_mcycle(gb, ld_hl_z);
}

// --------------------- ADD a, r8 --------------------

#define DEF_ADD(name)						\
static void add_a_##name (GB *gb) {				\
	uint8_t old = gb->cpu.a;				\
	gb->cpu.a += gb->cpu.name;				\
	set_z(gb, gb->cpu.a == 0);				\
	set_n(gb, 0);						\
	set_h(gb, (old & 0x0F) > (gb->cpu.a & 0x0F));		\
	set_c(gb, old > gb->cpu.a);				\
}

DEF_ADD(b)		// 0x80
DEF_ADD(c)		// 0x81
DEF_ADD(d)		// 0x82
DEF_ADD(e)		// 0x83
DEF_ADD(h)		// 0x84
DEF_ADD(l)		// 0x85
DEF_ADD(a)		// 0x87

static void add_a_hl (GB *gb) {	// 0x86
	uint8_t old = gb->cpu.a;
	gb->cpu.a += read8(gb, gb->cpu.hl);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old & 0x0F) > (gb->cpu.a & 0x0F));
	set_c(gb, old > gb->cpu.a);
}
DECODE8(add_a_hl)

#undef DEF_ADD

// --------------------- ADC a, r8 --------------------

#define DEF_ADC(name)					\
static void adc_a_##name (GB *gb) {			\
	uint8_t old = gb->cpu.a;			\
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;		\
	uint8_t op = gb->cpu.name;			\
	uint16_t r = (uint16_t)old + gb->cpu.name + c;	\
	gb->cpu.a = (uint8_t)r;				\
	set_z(gb, gb->cpu.a == 0);			\
	set_n(gb, 0);					\
	set_h(gb, (old ^ op ^ r) & 0x10);		\
	set_c(gb, r > 0xFF);				\
}

DEF_ADC(b)		// 0x88
DEF_ADC(c)		// 0x89
DEF_ADC(d)		// 0x8A
DEF_ADC(e)		// 0x8B
DEF_ADC(h)		// 0x8C
DEF_ADC(l)		// 0x8D
DEF_ADC(a)		// 0x8F

static void adc_a_hl (GB *gb) {		// 0x8E
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t v = read8(gb, gb->cpu.hl);
	uint16_t r = (uint16_t)old + v + c;
	gb->cpu.a = (uint8_t)r;		
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old ^ v ^ r) & 0x10);
	set_c(gb, r > 0xFF);
}
DECODE8(adc_a_hl)

#undef DEF_ADC

// --------------------- SUB a, r8 --------------------

#define DEF_SUB(name)				\
static void sub_a_##name (GB *gb) {		\
	uint8_t old = gb->cpu.a;		\
	uint8_t v = gb->cpu.name;		\
	gb->cpu.a -= v;				\
	set_z(gb, gb->cpu.a == 0);		\
	set_n(gb, 1);				\
	set_h(gb, (old & 0x0F) < (v & 0x0F));	\
	set_c(gb, old < v);			\
}

DEF_SUB(b)		// 0x90
DEF_SUB(c)		// 0x91
DEF_SUB(d)		// 0x92
DEF_SUB(e)		// 0x93
DEF_SUB(h)		// 0x94
DEF_SUB(l)		// 0x95
DEF_SUB(a)		// 0x97

static void sub_a_hl (GB *gb) {		// 0x96
	uint8_t old = gb->cpu.a;
	uint8_t v = read8(gb, gb->cpu.hl);
	gb->cpu.a -= v;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F));
	set_c(gb, old < v);
}
DECODE8(sub_a_hl)

#undef DEF_SUB

// --------------------- SBC a, r8 --------------------

#define DEF_SBC(name)					\
static void sbc_a_##name (GB *gb) {			\
	uint8_t old = gb->cpu.a;			\
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;		\
	uint8_t v = gb->cpu.name;			\
	gb->cpu.a = old - v - c;			\
	set_z(gb, gb->cpu.a == 0);			\
	set_n(gb, 1);					\
	set_h(gb, (old & 0x0F) < (v & 0x0F) + c);	\
	set_c(gb, (uint16_t)old < (uint16_t)v + c);	\
}

DEF_SBC(b)	// 0x98
DEF_SBC(c)	// 0x99
DEF_SBC(d)	// 0x9A
DEF_SBC(e)	// 0x9B
DEF_SBC(h)	// 0x9C
DEF_SBC(l)	// 0x9D
DEF_SBC(a)	// 0x9F

static void sbc_a_hl (GB *gb) {
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t v = read8(gb, gb->cpu.hl);
	gb->cpu.a = old - v - c;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F) + c);
	set_c(gb, (uint16_t)old < (uint16_t)v + c);
}
DECODE8(sbc_a_hl)

#undef DEF_SBC

// --------------------- AND a, r8 -------------------

#define DEF_AND(name)			\
static void and_a_##name (GB *gb) {	\
	gb->cpu.a &= gb->cpu.name;	\
	set_z(gb, gb->cpu.a == 0);	\
	set_n(gb, 0);			\
	set_h(gb, 1);			\
	set_c(gb, 0);			\
}

DEF_AND(b)		// 0xA0
DEF_AND(c)		// 0xA1
DEF_AND(d)		// 0xA2
DEF_AND(e)		// 0xA3
DEF_AND(h)		// 0xA4
DEF_AND(l)		// 0xA5
DEF_AND(a)		// 0xA7

static void and_a_hl (GB *gb) {		// 0xA6
	gb->cpu.a &= read8(gb, gb->cpu.hl);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 1);
	set_c(gb, 0);
}
DECODE8(and_a_hl)

#undef DEF_AND

// --------------------- XOR a, r8 -------------------

#define DEF_XOR(name)			\
static void xor_a_##name (GB *gb) {	\
	gb->cpu.a ^= gb->cpu.name;	\
	set_z(gb, gb->cpu.a == 0);	\
	set_n(gb, 0);			\
	set_h(gb, 0);			\
	set_c(gb, 0);			\
}

DEF_XOR(b)		// 0xA8
DEF_XOR(c)		// 0xA9
DEF_XOR(d)		// 0xAA
DEF_XOR(e)		// 0xAB
DEF_XOR(h)		// 0xAC
DEF_XOR(l)		// 0xAD
DEF_XOR(a)		// 0xAF

static void xor_a_hl (GB *gb) {		// 0xAE
	gb->cpu.a ^= read8(gb, gb->cpu.hl);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(xor_a_hl)

#undef DEF_XOR

// --------------------- OR a, r8 -------------------

#define DEF_OR(name)			\
static void or_a_##name (GB *gb) {	\
	gb->cpu.a |= gb->cpu.name;	\
	set_z(gb, gb->cpu.a == 0);	\
	set_n(gb, 0);			\
	set_h(gb, 0);			\
	set_c(gb, 0);			\
}

DEF_OR(b)		// 0xB0
DEF_OR(c)		// 0xB1
DEF_OR(d)		// 0xB2
DEF_OR(e)		// 0xB3
DEF_OR(h)		// 0xB4
DEF_OR(l)		// 0xB5
DEF_OR(a)		// 0xB7

static void or_a_hl (GB *gb) {		// 0xB6
	gb->cpu.a |= read8(gb, gb->cpu.hl);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(or_a_hl)

#undef DEF_OR

// ---------------------- CP a, r8 --------------------

#define DEF_CP(name)						\
static void cp_a_##name (GB *gb) {				\
	uint8_t tmp = gb->cpu.a - gb->cpu.name;			\
	set_z(gb, tmp == 0);					\
	set_n(gb, 1);						\
	set_h(gb, (gb->cpu.a & 0x0F) < (gb->cpu.name & 0x0F));	\
	set_c(gb, gb->cpu.a < gb->cpu.name);			\
}

DEF_CP(b)		// 0xB8
DEF_CP(c)		// 0xB9
DEF_CP(d)		// 0xBA
DEF_CP(e)		// 0xBB
DEF_CP(h)		// 0xBC
DEF_CP(l)		// 0xBD
DEF_CP(a)		// 0xBF

static void cp_a_hl (GB *gb) {		// 0xBE
	uint8_t v = read8(gb, gb->cpu.hl);
	uint8_t tmp = gb->cpu.a - v;	
	set_z(gb, tmp == 0);
	set_n(gb, 1);
	set_h(gb, (gb->cpu.a & 0x0F) < (v & 0x0F));
	set_c(gb, gb->cpu.a < v);
}
DECODE8(cp_a_hl)

#undef DEF_CP

// ---------------------- ADD a, imm8 ------------------

static void add_a_imm8 (GB *gb) {		// 0xC6
	uint8_t old = gb->cpu.a;
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.a += gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old & 0x0F) > (gb->cpu.a & 0x0F));
	set_c(gb, old > gb->cpu.a);
}
DECODE8(add_a_imm8)

// -------------------- ADC a, imm8 -------------------

static void adc_a_imm8 (GB *gb) {			// 0xCE
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t v = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	uint16_t r = (uint16_t)old + v + c;
	gb->cpu.a = (uint8_t)r;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old ^ r ^ v) & 0x10);
	set_c(gb, r > 0xFF);
}
DECODE8(adc_a_imm8)

// ------------------- SUB a, imm8 -------------------

static void sub_a_imm8 (GB *gb) {			// 0xD6
	uint8_t old = gb->cpu.a;
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t v = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	gb->cpu.a -= v;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F));
	set_c(gb, old < v);
}
DECODE8(sub_a_imm8)

// ------------------- SBC a, imm8 ------------------

static void sbc_a_imm8 (GB *gb) {			// 0xDE
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t v = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	gb->cpu.a = old - v - c;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F) + c);
	set_c(gb, (uint16_t)old < (uint16_t)v + c);
}
DECODE8(sbc_a_imm8)

// ------------------ AND a, imm8 ------------------

static void and_a_imm8 (GB *gb) {			// 0xE6
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.a &= gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 1);
	set_c(gb, 0);
}
DECODE8(and_a_imm8)

// ------------------ XOR a, imm8 -------------------

static void xor_a_imm8 (GB *gb) {			// 0xEE
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.a ^= gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(xor_a_imm8)

// ------------------- OR a, imm8 ------------------

static void or_a_imm8 (GB *gb) {			// 0xF6
	oam_bug(gb, gb->cpu.pc, 2);
	gb->cpu.a |= gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(or_a_imm8)

// ------------------- CP a, imm8 ------------------

static void cp_a_imm8 (GB *gb) {		// 0xFE
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t v = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	uint8_t tmp = gb->cpu.a - v;	
	set_z(gb, tmp == 0);
	set_n(gb, 1);
	set_h(gb, (gb->cpu.a & 0x0F) < (v & 0x0F));
	set_c(gb, gb->cpu.a < v);
}
DECODE8(cp_a_imm8)

// ------------------- ADD sp, imm8 --------------------

void add_sp_z (GB *gb) {		// 0xE8
	uint16_t old = gb->cpu.sp;
	int8_t v = (int8_t)gb->cpu.z;
	gb->cpu.sp = old + v;
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, ((old & 0xF) + (v & 0xF)) > 0xF);
	set_c(gb, ((old & 0xFF) + (v & 0xFF)) > 0xFF);
}
void decode_add_sp_imm8 (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, nop);
	push_mcycle(gb, add_sp_z);
}

// ---------------- init -----------------
void init_alu (OpcodeTable *t)
{
	t->main[0x33] = decode_inc16_sp;
	t->main[0x34] = decode_inc8_hl;
	t->main[0x35] = decode_dec8_hl;
	t->main[0x29] = decode_add_hl_hl;
	t->main[0x2B] = decode_dec16_hl;
	t->main[0x09] = decode_add_hl_bc;
	t->main[0x39] = decode_add_hl_sp;
	t->main[0x3B] = decode_dec16_sp;
	t->main[0x03] = decode_inc16_bc;
	t->main[0x0B] = decode_dec16_bc;
	t->main[0x13] = decode_inc16_de;
	t->main[0x19] = decode_add_hl_de;
	t->main[0x1B] = decode_dec16_de;
	t->main[0x23] = decode_inc16_hl;

	t->main[0x04] = inc8_b;
	t->main[0x05] = dec8_b;
	t->main[0x0C] = inc8_c;
	t->main[0x0D] = dec8_c;
	t->main[0x14] = inc8_d;
	t->main[0x15] = dec8_d;
	t->main[0x1C] = inc8_e;
	t->main[0x1D] = dec8_e;
	t->main[0x24] = inc8_h;
	t->main[0x25] = dec8_h;
	t->main[0x2C] = inc8_l;
	t->main[0x2D] = dec8_l;
	t->main[0x3C] = inc8_a;
	t->main[0x3D] = dec8_a;

	t->main[0x80] = add_a_b;
	t->main[0x81] = add_a_c;
	t->main[0x82] = add_a_d;
	t->main[0x83] = add_a_e;
	t->main[0x84] = add_a_h;
	t->main[0x85] = add_a_l;
	t->main[0x86] = decode_add_a_hl;
	t->main[0x87] = add_a_a;

	t->main[0x88] = adc_a_b;
	t->main[0x89] = adc_a_c;
	t->main[0x8A] = adc_a_d;
	t->main[0x8B] = adc_a_e;
	t->main[0x8C] = adc_a_h;
	t->main[0x8D] = adc_a_l;
	t->main[0x8E] = decode_adc_a_hl;
	t->main[0x8F] = adc_a_a;

	t->main[0x90] = sub_a_b;
	t->main[0x91] = sub_a_c;
	t->main[0x92] = sub_a_d;
	t->main[0x93] = sub_a_e;
	t->main[0x94] = sub_a_h;
	t->main[0x95] = sub_a_l;
	t->main[0x96] = decode_sub_a_hl;
	t->main[0x97] = sub_a_a;

	t->main[0x98] = sbc_a_b;
	t->main[0x99] = sbc_a_c;
	t->main[0x9A] = sbc_a_d;
	t->main[0x9B] = sbc_a_e;
	t->main[0x9C] = sbc_a_h;
	t->main[0x9D] = sbc_a_l;
	t->main[0x9E] = decode_sbc_a_hl;
	t->main[0x9F] = sbc_a_a;

	t->main[0xA0] = and_a_b;
	t->main[0xA1] = and_a_c;
	t->main[0xA2] = and_a_d;
	t->main[0xA3] = and_a_e;
	t->main[0xA4] = and_a_h;
	t->main[0xA5] = and_a_l;
	t->main[0xA6] = decode_and_a_hl;
	t->main[0xA7] = and_a_a;

	t->main[0xA8] = xor_a_b;
	t->main[0xA9] = xor_a_c;
	t->main[0xAA] = xor_a_d;
	t->main[0xAB] = xor_a_e;
	t->main[0xAC] = xor_a_h;
	t->main[0xAD] = xor_a_l;
	t->main[0xAE] = decode_xor_a_hl;
	t->main[0xAF] = xor_a_a;

	t->main[0xB0] = or_a_b;
	t->main[0xB1] = or_a_c;
	t->main[0xB2] = or_a_d;
	t->main[0xB3] = or_a_e;
	t->main[0xB4] = or_a_h;
	t->main[0xB5] = or_a_l;
	t->main[0xB6] = decode_or_a_hl;
	t->main[0xB7] = or_a_a;

	t->main[0xB8] = cp_a_b;
	t->main[0xB9] = cp_a_c;
	t->main[0xBA] = cp_a_d;
	t->main[0xBB] = cp_a_e;
	t->main[0xBC] = cp_a_h;
	t->main[0xBD] = cp_a_l;
	t->main[0xBE] = decode_cp_a_hl;
	t->main[0xBF] = cp_a_a;
	t->main[0xE8] = decode_add_sp_imm8;

	t->main[0xC6] = decode_add_a_imm8;
	t->main[0xCE] = decode_adc_a_imm8;
	t->main[0xD6] = decode_sub_a_imm8;
	t->main[0xDE] = decode_sbc_a_imm8;
	t->main[0xE6] = decode_and_a_imm8;
	t->main[0xEE] = decode_xor_a_imm8;
	t->main[0xF6] = decode_or_a_imm8;
	t->main[0xFE] = decode_cp_a_imm8;
}
