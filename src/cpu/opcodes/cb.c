#include "cpu/opcodes/common.h"

// ------------------ RLC r8 ------------------

#define DEF_RLC(name)					\
static void rlc_##name (GB *gb) {			\
	uint8_t bit = gb->cpu.name >> 7;		\
	gb->cpu.name = (gb->cpu.name << 1) | bit;	\
	set_c(gb, bit != 0);				\
	set_z(gb, gb->cpu.name == 0);			\
	set_n(gb, 0);					\
	set_h(gb, 0);					\
}

DEF_RLC(b)	// 0x00
DEF_RLC(c)	// 0x01
DEF_RLC(d)	// 0x02
DEF_RLC(e)	// 0x03
DEF_RLC(h)	// 0x04
DEF_RLC(l)	// 0x05
DEF_RLC(a)	// 0x07

static void rlc_hl (GB *gb) {			// 0x06
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl >> 7;
	uint8_t r = (hl << 1) | bit;
	gb->cpu.z = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RLC

// ------------------ RRC r8 ------------------

#define DEF_RRC(name)					\
static void rrc_##name (GB *gb) {			\
	uint8_t bit = gb->cpu.name << 7;		\
	gb->cpu.name = (gb->cpu.name >> 1) | bit;	\
	set_c(gb, bit != 0);				\
	set_z(gb, gb->cpu.name == 0);			\
	set_n(gb, 0);					\
	set_h(gb, 0);					\
}

DEF_RRC(b)	// 0x08
DEF_RRC(c)	// 0x09
DEF_RRC(d)	// 0x0A
DEF_RRC(e)	// 0x0B
DEF_RRC(h)	// 0x0C
DEF_RRC(l)	// 0x0D
DEF_RRC(a)	// 0x0F

static void rrc_hl (GB *gb) {			// 0x0E
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl << 7;
	uint8_t r = (hl >> 1) | bit;
	gb->cpu.z = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RRC

// -------------------------- RL r8 --------------------

#define DEF_RL(name)					\
static void rl_##name (GB *gb) {			\
	uint8_t new_carry = gb->cpu.name >> 7;		\
	uint8_t old_carry = (gb->cpu.f & FLAG_C) >> 4;	\
	gb->cpu.name = (gb->cpu.name << 1) | old_carry;	\
	set_c(gb, new_carry != 0);			\
	set_z(gb, gb->cpu.name == 0);			\
	set_n(gb, 0);					\
	set_h(gb, 0);					\
}

DEF_RL(b)	// 0x10
DEF_RL(c)	// 0x11
DEF_RL(d)	// 0x12
DEF_RL(e)	// 0x13
DEF_RL(h)	// 0x14
DEF_RL(l)	// 0x15
DEF_RL(a)	// 0x17

static void rl_hl (GB *gb) {			// 0x16
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t new_carry = hl >> 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t r = (hl << 1) | old_carry;

	gb->cpu.z = r;
	set_c(gb, new_carry != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RL

// ----------------------- RR r8 --------------------

#define DEF_RR(name)					\
static void rr_##name (GB *gb) {			\
	uint8_t new_carry = gb->cpu.name << 7;		\
	uint8_t old_carry = (gb->cpu.f & FLAG_C) << 3;	\
	gb->cpu.name = (gb->cpu.name >> 1) | old_carry;	\
	set_c(gb, new_carry != 0);			\
	set_z(gb, gb->cpu.name == 0);			\
	set_n(gb, 0);					\
	set_h(gb, 0);					\
}

DEF_RR(b)	// 0x18
DEF_RR(c)	// 0x19
DEF_RR(d)	// 0x1A
DEF_RR(e)	// 0x1B
DEF_RR(h)	// 0x1C
DEF_RR(l)	// 0x1D
DEF_RR(a)	// 0x1F

static void rr_hl (GB *gb) {			// 0x1E
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t new_carry = hl << 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) << 3;
	uint8_t r = (hl >> 1) | old_carry;

	gb->cpu.z = r;
	set_c(gb, new_carry != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RR

// -------------------- SLA r8 ------------------

#define DEF_SLA(name)				\
static void sla_##name (GB *gb) {		\
	uint8_t bit = gb->cpu.name & 0x80;	\
	gb->cpu.name <<= 1;			\
	set_c(gb, bit != 0);			\
	set_z(gb, gb->cpu.name == 0);		\
	set_n(gb, 0);				\
	set_h(gb, 0);				\
}

DEF_SLA(b)	// 0x20
DEF_SLA(c)	// 0x21
DEF_SLA(d)	// 0x22
DEF_SLA(e)	// 0x23
DEF_SLA(h)	// 0x24
DEF_SLA(l)	// 0x25
DEF_SLA(a)	// 0x27

static void sla_hl (GB *gb) {			// 0x26
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl & 0x80;
	uint8_t r = hl << 1;

	gb->cpu.z = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_SLA

// -------------------- SRA r8 ------------------

#define DEF_SRA(name)				\
static void sra_##name (GB *gb) {		\
	set_c(gb, gb->cpu.name & 1);		\
	uint8_t a = gb->cpu.name & 0x80;	\
	gb->cpu.name = (gb->cpu.name >> 1) | a;	\
	set_z(gb, gb->cpu.name == 0);		\
	set_n(gb, 0);				\
	set_h(gb, 0);				\
}

DEF_SRA(b)	// 0x28
DEF_SRA(c)	// 0x29
DEF_SRA(d)	// 0x2A
DEF_SRA(e)	// 0x2B
DEF_SRA(h)	// 0x2C
DEF_SRA(l)	// 0x2D
DEF_SRA(a)	// 0x2F

static void sra_hl (GB *gb) {			// 0x2E
	uint8_t hl = read8(gb, gb->cpu.hl);
	set_c(gb, hl & 1);
	uint8_t a = hl & 0x80;
	uint8_t r = (hl >> 1) | a;
	gb->cpu.z = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_SRA

// -------------------- SRL r8 ------------------

#define DEF_SRL(name)			\
static void srl_##name (GB *gb) {	\
	set_c(gb, gb->cpu.name & 1);	\
	gb->cpu.name >>= 1;		\
	set_z(gb, gb->cpu.name == 0);	\
	set_n(gb, 0);			\
	set_h(gb, 0);			\
}

DEF_SRL(b)	// 0x38
DEF_SRL(c)	// 0x39
DEF_SRL(d)	// 0x3A
DEF_SRL(e)	// 0x3B
DEF_SRL(h)	// 0x3C
DEF_SRL(l)	// 0x3D
DEF_SRL(a)	// 0x3F

static void srl_hl (GB *gb) {			// 0x3E
	uint8_t hl = read8(gb, gb->cpu.hl);
	set_c(gb, hl & 1);
	uint8_t r = hl >> 1;
	gb->cpu.z = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}


#undef DEF_SRL

// -------------------- SWAP r8 -------------------------

#define DEF_SWAP(name)			\
static void swap_##name (GB *gb) {	\
	uint8_t l = gb->cpu.name << 4;	\
	uint8_t h = gb->cpu.name >> 4;	\
	gb->cpu.name = h | l;		\
	set_z(gb, gb->cpu.name == 0);	\
	set_n(gb, 0);			\
	set_h(gb, 0);			\
	set_c(gb, 0);			\
}

DEF_SWAP(b)	// 0x30
DEF_SWAP(c)	// 0x31
DEF_SWAP(d)	// 0x32
DEF_SWAP(e)	// 0x33
DEF_SWAP(h)	// 0x34
DEF_SWAP(l)	// 0x35
DEF_SWAP(a)	// 0x37

static void swap_hl (GB *gb) {		// 0x36
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t r = (hl >> 4) | (hl << 4);
	gb->cpu.z = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}

#undef DEF_SWAP

// --------------------- BIT b3, r8 ------------------

#define DEF_BIT(idx, reg)			\
static void bit_##idx##_##reg (GB *gb) {	\
	uint8_t a = gb->cpu.reg & (1 << idx);	\
	set_z(gb, !a);				\
	set_n(gb, 0);				\
	set_h(gb, 1);				\
}

DEF_BIT(0, b)		// 0x40
DEF_BIT(0, c)		// 0x41
DEF_BIT(0, d)		// 0x42
DEF_BIT(0, e)		// 0x43
DEF_BIT(0, h)		// 0x44
DEF_BIT(0, l)		// 0x45
DEF_BIT(0, a)		// 0x47

DEF_BIT(1, b)		// 0x48
DEF_BIT(1, c)		// 0x49
DEF_BIT(1, d)		// 0x4A
DEF_BIT(1, e)		// 0x4B
DEF_BIT(1, h)		// 0x4C
DEF_BIT(1, l)		// 0x4D
DEF_BIT(1, a)		// 0x4F

DEF_BIT(2, b)		// 0x50
DEF_BIT(2, c)		// 0x51
DEF_BIT(2, d)		// 0x52
DEF_BIT(2, e)		// 0x53
DEF_BIT(2, h)		// 0x54
DEF_BIT(2, l)		// 0x55
DEF_BIT(2, a)		// 0x57

DEF_BIT(3, b)		// 0x58
DEF_BIT(3, c)		// 0x59
DEF_BIT(3, d)		// 0x5A
DEF_BIT(3, e)		// 0x5B
DEF_BIT(3, h)		// 0x5C
DEF_BIT(3, l)		// 0x5D
DEF_BIT(3, a)		// 0x5F

DEF_BIT(4, b)		// 0x60
DEF_BIT(4, c)		// 0x61
DEF_BIT(4, d)		// 0x62
DEF_BIT(4, e)		// 0x63
DEF_BIT(4, h)		// 0x64
DEF_BIT(4, l)		// 0x65
DEF_BIT(4, a)		// 0x67

DEF_BIT(5, b)		// 0x68
DEF_BIT(5, c)		// 0x69
DEF_BIT(5, d)		// 0x6A
DEF_BIT(5, e)		// 0x6B
DEF_BIT(5, h)		// 0x6C
DEF_BIT(5, l)		// 0x6D
DEF_BIT(5, a)		// 0x6F

DEF_BIT(6, b)		// 0x70
DEF_BIT(6, c)		// 0x71
DEF_BIT(6, d)		// 0x72
DEF_BIT(6, e)		// 0x73
DEF_BIT(6, h)		// 0x74
DEF_BIT(6, l)		// 0x75
DEF_BIT(6, a)		// 0x77

DEF_BIT(7, b)		// 0x78
DEF_BIT(7, c)		// 0x79
DEF_BIT(7, d)		// 0x7A
DEF_BIT(7, e)		// 0x7B
DEF_BIT(7, h)		// 0x7C
DEF_BIT(7, l)		// 0x7D
DEF_BIT(7, a)		// 0x7F

#undef DEF_BIT

#define DEF_BIT_HL(idx)					\
static void bit_##idx##_hl (GB *gb) {			\
	uint8_t a = read8(gb, gb->cpu.hl) & (1 << idx);	\
	set_z(gb, !a);					\
	set_n(gb, 0);					\
	set_h(gb, 1);					\
}

DEF_BIT_HL(0)		// 0x46
DEF_BIT_HL(1)		// 0x4E
DEF_BIT_HL(2)		// 0x56
DEF_BIT_HL(3)		// 0x5E
DEF_BIT_HL(4)		// 0x66
DEF_BIT_HL(5)		// 0x6E
DEF_BIT_HL(6)		// 0x76
DEF_BIT_HL(7)		// 0x7E

#undef DEF_BIT_HL

// --------------------- RES b3, r8 ------------------

#define DEF_RES(idx, reg)			\
static void res_##idx##_##reg (GB *gb) {	\
	gb->cpu.reg &= ~(1 << idx);		\
}

DEF_RES(0, b)		// 0x80
DEF_RES(0, c)		// 0x81
DEF_RES(0, d)		// 0x82
DEF_RES(0, e)		// 0x83
DEF_RES(0, h)		// 0x84
DEF_RES(0, l)		// 0x85
DEF_RES(0, a)		// 0x87

DEF_RES(1, b)		// 0x88
DEF_RES(1, c)		// 0x89
DEF_RES(1, d)		// 0x8A
DEF_RES(1, e)		// 0x8B
DEF_RES(1, h)		// 0x8C
DEF_RES(1, l)		// 0x8D
DEF_RES(1, a)		// 0x8F

DEF_RES(2, b)		// 0x90
DEF_RES(2, c)		// 0x91
DEF_RES(2, d)		// 0x92
DEF_RES(2, e)		// 0x93
DEF_RES(2, h)		// 0x94
DEF_RES(2, l)		// 0x95
DEF_RES(2, a)		// 0x97

DEF_RES(3, b)		// 0x98
DEF_RES(3, c)		// 0x99
DEF_RES(3, d)		// 0x9A
DEF_RES(3, e)		// 0x9B
DEF_RES(3, h)		// 0x9C
DEF_RES(3, l)		// 0x9D
DEF_RES(3, a)		// 0x9F

DEF_RES(4, b)		// 0xA0
DEF_RES(4, c)		// 0xA1
DEF_RES(4, d)		// 0xA2
DEF_RES(4, e)		// 0xA3
DEF_RES(4, h)		// 0xA4
DEF_RES(4, l)		// 0xA5
DEF_RES(4, a)		// 0xA7

DEF_RES(5, b)		// 0xA8
DEF_RES(5, c)		// 0xA9
DEF_RES(5, d)		// 0xAA
DEF_RES(5, e)		// 0xAB
DEF_RES(5, h)		// 0xAC
DEF_RES(5, l)		// 0xAD
DEF_RES(5, a)		// 0xAF

DEF_RES(6, b)		// 0xB0
DEF_RES(6, c)		// 0xB1
DEF_RES(6, d)		// 0xB2
DEF_RES(6, e)		// 0xB3
DEF_RES(6, h)		// 0xB4
DEF_RES(6, l)		// 0xB5
DEF_RES(6, a)		// 0xB7

DEF_RES(7, b)		// 0xB8
DEF_RES(7, c)		// 0xB9
DEF_RES(7, d)		// 0xBA
DEF_RES(7, e)		// 0xBB
DEF_RES(7, h)		// 0xBC
DEF_RES(7, l)		// 0xBD
DEF_RES(7, a)		// 0xBF

#undef DEF_RES

#define DEF_RES_HL(idx)				\
static void res_##idx##_hl (GB *gb) {		\
	uint8_t hl = read8(gb, gb->cpu.hl);	\
	gb->cpu.z = hl & ~(1 << idx);		\
}

DEF_RES_HL(0)		// 0x86
DEF_RES_HL(1)		// 0x8E
DEF_RES_HL(2)		// 0x96
DEF_RES_HL(3)		// 0x9E
DEF_RES_HL(4)		// 0xA6
DEF_RES_HL(5)		// 0xAE
DEF_RES_HL(6)		// 0xB6
DEF_RES_HL(7)		// 0xBE

#undef DEF_RES_HL

// --------------------- SET b3, r8 ------------------

#define DEF_SET(idx, reg)			\
static void set_##idx##_##reg (GB *gb) {	\
	gb->cpu.reg |= 1 << idx;		\
}

DEF_SET(0, b)		// 0xC0
DEF_SET(0, c)		// 0xC1
DEF_SET(0, d)		// 0xC2
DEF_SET(0, e)		// 0xC3
DEF_SET(0, h)		// 0xC4
DEF_SET(0, l)		// 0xC5
DEF_SET(0, a)		// 0xC7

DEF_SET(1, b)		// 0xC8
DEF_SET(1, c)		// 0xC9
DEF_SET(1, d)		// 0xCA
DEF_SET(1, e)		// 0xCB
DEF_SET(1, h)		// 0xCC
DEF_SET(1, l)		// 0xCD
DEF_SET(1, a)		// 0xCF

DEF_SET(2, b)		// 0xD0
DEF_SET(2, c)		// 0xD1
DEF_SET(2, d)		// 0xD2
DEF_SET(2, e)		// 0xD3
DEF_SET(2, h)		// 0xD4
DEF_SET(2, l)		// 0xD5
DEF_SET(2, a)		// 0xD7

DEF_SET(3, b)		// 0xD8
DEF_SET(3, c)		// 0xD9
DEF_SET(3, d)		// 0xDA
DEF_SET(3, e)		// 0xDB
DEF_SET(3, h)		// 0xDC
DEF_SET(3, l)		// 0xDD
DEF_SET(3, a)		// 0xDF

DEF_SET(4, b)		// 0xE0
DEF_SET(4, c)		// 0xE1
DEF_SET(4, d)		// 0xE2
DEF_SET(4, e)		// 0xE3
DEF_SET(4, h)		// 0xE4
DEF_SET(4, l)		// 0xE5
DEF_SET(4, a)		// 0xE7

DEF_SET(5, b)		// 0xE8
DEF_SET(5, c)		// 0xE9
DEF_SET(5, d)		// 0xEA
DEF_SET(5, e)		// 0xEB
DEF_SET(5, h)		// 0xEC
DEF_SET(5, l)		// 0xED
DEF_SET(5, a)		// 0xEF

DEF_SET(6, b)		// 0xF0
DEF_SET(6, c)		// 0xF1
DEF_SET(6, d)		// 0xF2
DEF_SET(6, e)		// 0xF3
DEF_SET(6, h)		// 0xF4
DEF_SET(6, l)		// 0xF5
DEF_SET(6, a)		// 0xF7

DEF_SET(7, b)		// 0xF8
DEF_SET(7, c)		// 0xF9
DEF_SET(7, d)		// 0xFA
DEF_SET(7, e)		// 0xFB
DEF_SET(7, h)		// 0xFC
DEF_SET(7, l)		// 0xFD
DEF_SET(7, a)		// 0xFF

#undef DEF_SET

#define DEF_SET_HL(idx)				\
static void set_##idx##_hl (GB *gb) {		\
	uint8_t hl = read8(gb, gb->cpu.hl);	\
	gb->cpu.z = hl | (1 << idx);		\
}

DEF_SET_HL(0)		// 0xC6
DEF_SET_HL(1)		// 0xCE
DEF_SET_HL(2)		// 0xD6
DEF_SET_HL(3)		// 0xDE
DEF_SET_HL(4)		// 0xE6
DEF_SET_HL(5)		// 0xEE
DEF_SET_HL(6)		// 0xF6
DEF_SET_HL(7)		// 0xFE

#undef DEF_SET_HL

void init_cb (OpcodeTable *t)
{
	// 0x00 - 0x07 RLC
	t->cb[0x00] = rlc_b;
	t->cb[0x01] = rlc_c;
	t->cb[0x02] = rlc_d;
	t->cb[0x03] = rlc_e;
	t->cb[0x04] = rlc_h;
	t->cb[0x05] = rlc_l;
	t->cb[0x06] = rlc_hl;
	t->cb[0x07] = rlc_a;

	// 0x08 - 0x0F RRC
	t->cb[0x08] = rrc_b;
	t->cb[0x09] = rrc_c;
	t->cb[0x0A] = rrc_d;
	t->cb[0x0B] = rrc_e;
	t->cb[0x0C] = rrc_h;
	t->cb[0x0D] = rrc_l;
	t->cb[0x0E] = rrc_hl;
	t->cb[0x0F] = rrc_a;

	// 0x10 - 0x17 RL
	t->cb[0x10] = rl_b;
	t->cb[0x11] = rl_c;
	t->cb[0x12] = rl_d;
	t->cb[0x13] = rl_e;
	t->cb[0x14] = rl_h;
	t->cb[0x15] = rl_l;
	t->cb[0x16] = rl_hl;
	t->cb[0x17] = rl_a;

	// 0x18 - 0x1F RR
	t->cb[0x18] = rr_b;
	t->cb[0x19] = rr_c;
	t->cb[0x1A] = rr_d;
	t->cb[0x1B] = rr_e;
	t->cb[0x1C] = rr_h;
	t->cb[0x1D] = rr_l;
	t->cb[0x1E] = rr_hl;
	t->cb[0x1F] = rr_a;

	// 0x20 - 0x27 SLA
	t->cb[0x20] = sla_b;
	t->cb[0x21] = sla_c;
	t->cb[0x22] = sla_d;
	t->cb[0x23] = sla_e;
	t->cb[0x24] = sla_h;
	t->cb[0x25] = sla_l;
	t->cb[0x26] = sla_hl;
	t->cb[0x27] = sla_a;

	// 0x28 - 0x2F SRA
	t->cb[0x28] = sra_b;
	t->cb[0x29] = sra_c;
	t->cb[0x2A] = sra_d;
	t->cb[0x2B] = sra_e;
	t->cb[0x2C] = sra_h;
	t->cb[0x2D] = sra_l;
	t->cb[0x2E] = sra_hl;
	t->cb[0x2F] = sra_a;

	// 0x30 - 0x3F SWAP / SRL
	t->cb[0x30] = swap_b;
	t->cb[0x31] = swap_c;
	t->cb[0x32] = swap_d;
	t->cb[0x33] = swap_e;
	t->cb[0x34] = swap_h;
	t->cb[0x35] = swap_l;
	t->cb[0x36] = swap_hl;
	t->cb[0x37] = swap_a;

	t->cb[0x38] = srl_b;
	t->cb[0x39] = srl_c;
	t->cb[0x3A] = srl_d;
	t->cb[0x3B] = srl_e;
	t->cb[0x3C] = srl_h;
	t->cb[0x3D] = srl_l;
	t->cb[0x3E] = srl_hl;
	t->cb[0x3F] = srl_a;

	// 0x40 - 0x7F BIT
	t->cb[0x40] = bit_0_b;
	t->cb[0x41] = bit_0_c;
	t->cb[0x42] = bit_0_d;
	t->cb[0x43] = bit_0_e;
	t->cb[0x44] = bit_0_h;
	t->cb[0x45] = bit_0_l;
	t->cb[0x46] = bit_0_hl;
	t->cb[0x47] = bit_0_a;

	t->cb[0x48] = bit_1_b;
	t->cb[0x49] = bit_1_c;
	t->cb[0x4A] = bit_1_d;
	t->cb[0x4B] = bit_1_e;
	t->cb[0x4C] = bit_1_h;
	t->cb[0x4D] = bit_1_l;
	t->cb[0x4E] = bit_1_hl;
	t->cb[0x4F] = bit_1_a;

	t->cb[0x50] = bit_2_b;
	t->cb[0x51] = bit_2_c;
	t->cb[0x52] = bit_2_d;
	t->cb[0x53] = bit_2_e;
	t->cb[0x54] = bit_2_h;
	t->cb[0x55] = bit_2_l;
	t->cb[0x56] = bit_2_hl;
	t->cb[0x57] = bit_2_a;

	t->cb[0x58] = bit_3_b;
	t->cb[0x59] = bit_3_c;
	t->cb[0x5A] = bit_3_d;
	t->cb[0x5B] = bit_3_e;
	t->cb[0x5C] = bit_3_h;
	t->cb[0x5D] = bit_3_l;
	t->cb[0x5E] = bit_3_hl;
	t->cb[0x5F] = bit_3_a;

	t->cb[0x60] = bit_4_b;
	t->cb[0x61] = bit_4_c;
	t->cb[0x62] = bit_4_d;
	t->cb[0x63] = bit_4_e;
	t->cb[0x64] = bit_4_h;
	t->cb[0x65] = bit_4_l;
	t->cb[0x66] = bit_4_hl;
	t->cb[0x67] = bit_4_a;

	t->cb[0x68] = bit_5_b;
	t->cb[0x69] = bit_5_c;
	t->cb[0x6A] = bit_5_d;
	t->cb[0x6B] = bit_5_e;
	t->cb[0x6C] = bit_5_h;
	t->cb[0x6D] = bit_5_l;
	t->cb[0x6E] = bit_5_hl;
	t->cb[0x6F] = bit_5_a;

	t->cb[0x70] = bit_6_b;
	t->cb[0x71] = bit_6_c;
	t->cb[0x72] = bit_6_d;
	t->cb[0x73] = bit_6_e;
	t->cb[0x74] = bit_6_h;
	t->cb[0x75] = bit_6_l;
	t->cb[0x76] = bit_6_hl;
	t->cb[0x77] = bit_6_a;

	t->cb[0x78] = bit_7_b;
	t->cb[0x79] = bit_7_c;
	t->cb[0x7A] = bit_7_d;
	t->cb[0x7B] = bit_7_e;
	t->cb[0x7C] = bit_7_h;
	t->cb[0x7D] = bit_7_l;
	t->cb[0x7E] = bit_7_hl;
	t->cb[0x7F] = bit_7_a;


	// -------------------- RES 0x80 - 0xBF --------------------

	t->cb[0x80] = res_0_b;
	t->cb[0x81] = res_0_c;
	t->cb[0x82] = res_0_d;
	t->cb[0x83] = res_0_e;
	t->cb[0x84] = res_0_h;
	t->cb[0x85] = res_0_l;
	t->cb[0x86] = res_0_hl;
	t->cb[0x87] = res_0_a;

	t->cb[0x88] = res_1_b;
	t->cb[0x89] = res_1_c;
	t->cb[0x8A] = res_1_d;
	t->cb[0x8B] = res_1_e;
	t->cb[0x8C] = res_1_h;
	t->cb[0x8D] = res_1_l;
	t->cb[0x8E] = res_1_hl;
	t->cb[0x8F] = res_1_a;

	t->cb[0x90] = res_2_b;
	t->cb[0x91] = res_2_c;
	t->cb[0x92] = res_2_d;
	t->cb[0x93] = res_2_e;
	t->cb[0x94] = res_2_h;
	t->cb[0x95] = res_2_l;
	t->cb[0x96] = res_2_hl;
	t->cb[0x97] = res_2_a;

	t->cb[0x98] = res_3_b;
	t->cb[0x99] = res_3_c;
	t->cb[0x9A] = res_3_d;
	t->cb[0x9B] = res_3_e;
	t->cb[0x9C] = res_3_h;
	t->cb[0x9D] = res_3_l;
	t->cb[0x9E] = res_3_hl;
	t->cb[0x9F] = res_3_a;

	t->cb[0xA0] = res_4_b;
	t->cb[0xA1] = res_4_c;
	t->cb[0xA2] = res_4_d;
	t->cb[0xA3] = res_4_e;
	t->cb[0xA4] = res_4_h;
	t->cb[0xA5] = res_4_l;
	t->cb[0xA6] = res_4_hl;
	t->cb[0xA7] = res_4_a;

	t->cb[0xA8] = res_5_b;
	t->cb[0xA9] = res_5_c;
	t->cb[0xAA] = res_5_d;
	t->cb[0xAB] = res_5_e;
	t->cb[0xAC] = res_5_h;
	t->cb[0xAD] = res_5_l;
	t->cb[0xAE] = res_5_hl;
	t->cb[0xAF] = res_5_a;

	t->cb[0xB0] = res_6_b;
	t->cb[0xB1] = res_6_c;
	t->cb[0xB2] = res_6_d;
	t->cb[0xB3] = res_6_e;
	t->cb[0xB4] = res_6_h;
	t->cb[0xB5] = res_6_l;
	t->cb[0xB6] = res_6_hl;
	t->cb[0xB7] = res_6_a;

	t->cb[0xB8] = res_7_b;
	t->cb[0xB9] = res_7_c;
	t->cb[0xBA] = res_7_d;
	t->cb[0xBB] = res_7_e;
	t->cb[0xBC] = res_7_h;
	t->cb[0xBD] = res_7_l;
	t->cb[0xBE] = res_7_hl;
	t->cb[0xBF] = res_7_a;


	// -------------------- SET 0xC0 - 0xFF --------------------

	t->cb[0xC0] = set_0_b;
	t->cb[0xC1] = set_0_c;
	t->cb[0xC2] = set_0_d;
	t->cb[0xC3] = set_0_e;
	t->cb[0xC4] = set_0_h;
	t->cb[0xC5] = set_0_l;
	t->cb[0xC6] = set_0_hl;
	t->cb[0xC7] = set_0_a;

	t->cb[0xC8] = set_1_b;
	t->cb[0xC9] = set_1_c;
	t->cb[0xCA] = set_1_d;
	t->cb[0xCB] = set_1_e;
	t->cb[0xCC] = set_1_h;
	t->cb[0xCD] = set_1_l;
	t->cb[0xCE] = set_1_hl;
	t->cb[0xCF] = set_1_a;

	t->cb[0xD0] = set_2_b;
	t->cb[0xD1] = set_2_c;
	t->cb[0xD2] = set_2_d;
	t->cb[0xD3] = set_2_e;
	t->cb[0xD4] = set_2_h;
	t->cb[0xD5] = set_2_l;
	t->cb[0xD6] = set_2_hl;
	t->cb[0xD7] = set_2_a;

	t->cb[0xD8] = set_3_b;
	t->cb[0xD9] = set_3_c;
	t->cb[0xDA] = set_3_d;
	t->cb[0xDB] = set_3_e;
	t->cb[0xDC] = set_3_h;
	t->cb[0xDD] = set_3_l;
	t->cb[0xDE] = set_3_hl;
	t->cb[0xDF] = set_3_a;

	t->cb[0xE0] = set_4_b;
	t->cb[0xE1] = set_4_c;
	t->cb[0xE2] = set_4_d;
	t->cb[0xE3] = set_4_e;
	t->cb[0xE4] = set_4_h;
	t->cb[0xE5] = set_4_l;
	t->cb[0xE6] = set_4_hl;
	t->cb[0xE7] = set_4_a;

	t->cb[0xE8] = set_5_b;
	t->cb[0xE9] = set_5_c;
	t->cb[0xEA] = set_5_d;
	t->cb[0xEB] = set_5_e;
	t->cb[0xEC] = set_5_h;
	t->cb[0xED] = set_5_l;
	t->cb[0xEE] = set_5_hl;
	t->cb[0xEF] = set_5_a;

	t->cb[0xF0] = set_6_b;
	t->cb[0xF1] = set_6_c;
	t->cb[0xF2] = set_6_d;
	t->cb[0xF3] = set_6_e;
	t->cb[0xF4] = set_6_h;
	t->cb[0xF5] = set_6_l;
	t->cb[0xF6] = set_6_hl;
	t->cb[0xF7] = set_6_a;

	t->cb[0xF8] = set_7_b;
	t->cb[0xF9] = set_7_c;
	t->cb[0xFA] = set_7_d;
	t->cb[0xFB] = set_7_e;
	t->cb[0xFC] = set_7_h;
	t->cb[0xFD] = set_7_l;
	t->cb[0xFE] = set_7_hl;
	t->cb[0xFF] = set_7_a;
}






















