#include <cpu/opcodes/common.h>

// ------------------------------- ld [r16mem], a ------------------------------------

static void ld_bcmem_a (GB *gb) {			// 0x02
	write8(gb, gb->cpu.bc, gb->cpu.a);
}
DECODE8(ld_bcmem_a)

static void ld_demem_a (GB *gb) {			// 0x12
	write8(gb, gb->cpu.de, gb->cpu.a);
}
DECODE8(ld_demem_a)

static void ld_hlimem_a (GB *gb) {			// 0x22
	write8(gb, gb->cpu.hl, gb->cpu.a);
	gb->cpu.hl++;
}
DECODE8(ld_hlimem_a)

static void ld_hldmem_a (GB *gb) {			// 0x32
	write8(gb, gb->cpu.hl, gb->cpu.a);
	gb->cpu.hl--;
}
DECODE8(ld_hldmem_a)

// ------------------- ld [imm16], sp ------------------

static void decode_ld_imm16_sp (GB *gb) {	// 0x08
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_w_imm8);
	push_mcycle(gb, ld_wzmem_sp_low);
	push_mcycle(gb, ld_wzmem_sp_high);
}

// ------------------ ld r8, imm8 ---------------------

#define DEF_LD_NEXT8(name)					\
static void ld_##name##_imm8 (GB *gb) {				\
	oam_bug(gb, gb->cpu.pc, 2);				\
	gb->cpu.name = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);\
}

DEF_LD_NEXT8(b)		// 0x06
DEF_LD_NEXT8(c)		// 0x0E
DEF_LD_NEXT8(d)		// 0x16
DEF_LD_NEXT8(e)		// 0x1E
DEF_LD_NEXT8(h)		// 0x26
DEF_LD_NEXT8(l)		// 0x2E
DEF_LD_NEXT8(a)		// 0x3E

#undef DEF_LD_NEXT8

#define DEF_DECODE_LD_NEXT8(name)		\
static void decode_ld_##name##_imm8 (GB *gb) {	\
	push_mcycle(gb, ld_##name##_imm8);	\
}

DEF_DECODE_LD_NEXT8(b)
DEF_DECODE_LD_NEXT8(c)
DEF_DECODE_LD_NEXT8(d)
DEF_DECODE_LD_NEXT8(e)
DEF_DECODE_LD_NEXT8(h)
DEF_DECODE_LD_NEXT8(l)
DEF_DECODE_LD_NEXT8(a)

static void decode_ld_hl_imm8 (GB *gb) {	// 0x36
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_hl_z);
}

#undef DEF_DECODE_LD_NEXT8

// --------------------------- ld r16, imm16 ---------------------

static void decode_ld_bc_imm16 (GB *gb) {
	push_mcycle(gb, ld_c_imm8);
	push_mcycle(gb, ld_b_imm8);
}
static void decode_ld_de_imm16 (GB *gb) {
	push_mcycle(gb, ld_e_imm8);
	push_mcycle(gb, ld_d_imm8);
}
static void decode_ld_hl_imm16 (GB *gb) {
	push_mcycle(gb, ld_l_imm8);
	push_mcycle(gb, ld_h_imm8);
}

static void decode_ld_sp_imm16 (GB *gb) {
	push_mcycle(gb, ld_sp_low_imm8);
	push_mcycle(gb, ld_sp_high_imm8);
}

// --------------------------------- ld a, [r16mem] ----------------------------

static void ld_a_bcmem (GB *gb) {			// 0x0A
	gb->cpu.a = read8(gb, gb->cpu.bc);
}
DECODE8(ld_a_bcmem)

static void ld_a_demem (GB *gb) {			// 0x1A
	gb->cpu.a = read8(gb, gb->cpu.de);
}
DECODE8(ld_a_demem)

static void ld_a_hlimem (GB *gb) {			// 0x2A
	gb->cpu.a = gb->cpu.bus->read8(gb->cpu.bus->ctx, gb->cpu.hl);
	oam_bug(gb, gb->cpu.hl, 2);
	gb->cpu.hl++;
}
DECODE8(ld_a_hlimem)

static void ld_a_hldmem (GB *gb) {			// 0x3A
	gb->cpu.a = gb->cpu.bus->read8(gb->cpu.bus->ctx, gb->cpu.hl);
	oam_bug(gb, gb->cpu.hl, 2);
	gb->cpu.hl--;
}
DECODE8(ld_a_hldmem)

// -------------------- LD r8, r8 ----------------------

#define DEF_LD(dest, src)			\
static void ld_##dest##_##src (GB *gb) {	\
	gb->cpu.dest = gb->cpu.src;		\
}

DEF_LD(b, b)	// 0x40
DEF_LD(b, c)	// 0x41
DEF_LD(b, d)	// 0x42
DEF_LD(b, e)	// 0x43
DEF_LD(b, h)	// 0x44
DEF_LD(b, l)	// 0x45
DEF_LD(b, a)	// 0x47

DEF_LD(c, b)	// 0x48
DEF_LD(c, c)	// 0x49
DEF_LD(c, d)	// 0x4A
DEF_LD(c, e)	// 0x4B
DEF_LD(c, h)	// 0x4C
DEF_LD(c, l)	// 0x4D
DEF_LD(c, a)	// 0x4F

DEF_LD(d, b)	// 0x50
DEF_LD(d, c)	// 0x51
DEF_LD(d, d)	// 0x52
DEF_LD(d, e)	// 0x53
DEF_LD(d, h)	// 0x54
DEF_LD(d, l)	// 0x55
DEF_LD(d, a)	// 0x57

DEF_LD(e, b)	// 0x58
DEF_LD(e, c)	// 0x59
DEF_LD(e, d)	// 0x5A
DEF_LD(e, e)	// 0x5B
DEF_LD(e, h)	// 0x5C
DEF_LD(e, l)	// 0x5D
DEF_LD(e, a)	// 0x5F

DEF_LD(h, b)	// 0x60
DEF_LD(h, c)	// 0x61
DEF_LD(h, d)	// 0x62
DEF_LD(h, e)	// 0x63
DEF_LD(h, h)	// 0x64
DEF_LD(h, l)	// 0x65
DEF_LD(h, a)	// 0x67

DEF_LD(l, b)	// 0x68
DEF_LD(l, c)	// 0x69
DEF_LD(l, d)	// 0x6A
DEF_LD(l, e)	// 0x6B
DEF_LD(l, h)	// 0x6C
DEF_LD(l, l)	// 0x6D
DEF_LD(l, a)	// 0x6F

DEF_LD(a, b)	// 0x78
DEF_LD(a, c)	// 0x79
DEF_LD(a, d)	// 0x7A
DEF_LD(a, e)	// 0x7B
DEF_LD(a, h)	// 0x7C
DEF_LD(a, l)	// 0x7D
DEF_LD(a, a)	// 0x7F

#undef DEF_LD

// --------------------- ld r8, (hl) --------------------

#define DEF_LD_HL(name)				\
static void ld_##name##_hl (GB *gb) {		\
	gb->cpu.name = read8(gb, gb->cpu.hl);	\
}

DEF_LD_HL(b)	// 0x46
DEF_LD_HL(c)	// 0x4E
DEF_LD_HL(d)	// 0x56
DEF_LD_HL(e)	// 0x5E
DEF_LD_HL(h)	// 0x66
DEF_LD_HL(l)	// 0x6E
DEF_LD_HL(a)	// 0x7E

#undef DEF_LD_HL

#define DECODE_DEF_LD_HL(name)			\
static void decode_ld_##name##_hl (GB *gb) {	\
	push_mcycle(gb, ld_##name##_hl);	\
}

DECODE_DEF_LD_HL(b)	// 0x46
DECODE_DEF_LD_HL(c)	// 0x4E
DECODE_DEF_LD_HL(d)	// 0x56
DECODE_DEF_LD_HL(e)	// 0x5E
DECODE_DEF_LD_HL(h)	// 0x66
DECODE_DEF_LD_HL(l)	// 0x6E
DECODE_DEF_LD_HL(a)	// 0x7E

#undef DECODE_DEF_LD_HL

// -------------------- ld (hl), r8 ---------------------

#define DEF_LD_HL(name)				\
static void ld_hl_##name (GB *gb) {		\
	write8(gb, gb->cpu.hl, gb->cpu.name);	\
}

DEF_LD_HL(b)	// 0x70
DEF_LD_HL(c)	// 0x71
DEF_LD_HL(d)	// 0x72
DEF_LD_HL(e)	// 0x73
DEF_LD_HL(h)	// 0x74
DEF_LD_HL(l)	// 0x75
DEF_LD_HL(a)	// 0x77

#undef DEF_LD_HL

#define DECODE_DEF_LD_HL(name)			\
static void decode_ld_hl_##name (GB *gb) {	\
	push_mcycle(gb, ld_hl_##name);		\
}

DECODE_DEF_LD_HL(b)	// 0x70
DECODE_DEF_LD_HL(c)	// 0x71
DECODE_DEF_LD_HL(d)	// 0x72
DECODE_DEF_LD_HL(e)	// 0x73
DECODE_DEF_LD_HL(h)	// 0x74
DECODE_DEF_LD_HL(l)	// 0x75
DECODE_DEF_LD_HL(a)	// 0x77

#undef DECODE_DEF_LD_HL

// ---------------------- LDH [c], a -----------------------

static void ldh_c_a (GB *gb) {			// 0xE2
	write8(gb, gb->cpu.c + 0xFF00, gb->cpu.a);
}
DECODE8(ldh_c_a)

// ---------------------- LDH a, [c] -----------------------

static void ldh_a_c (GB *gb) {			// 0xF2
	gb->cpu.a = read8(gb, gb->cpu.c + 0xFF00);
}
DECODE8(ldh_a_c)

// --------------------- LDH [imm8], a ---------------------

static void ldh_z_a (GB *gb) {		// 0xE0
	write8(gb, 0xFF00 + gb->cpu.z, gb->cpu.a);
}
static void decode_ldh_imm8_a (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ldh_z_a);
}

// -------------------- LDH a, [imm8] ----------------------

static void ldh_a_z (GB *gb) {		// 0xF0
	gb->cpu.a = read8(gb, 0xFF00 + gb->cpu.z);
}
static void decode_ldh_a_imm8 (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ldh_a_z);
}

// ------------------- LD [imm16], a --------------------

static void ld_wz_a (GB *gb) {		// 0xEA
	write8(gb, gb->cpu.wz, gb->cpu.a);
}
static void decode_ld_imm16_a (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_w_imm8);
	push_mcycle(gb, ld_wz_a);
}

// ------------------- LD a, [imm16] --------------------

static void ld_a_wz (GB *gb) {		// 0xFA
	gb->cpu.a = read8(gb, gb->cpu.wz);
}
static void decode_ld_a_imm16 (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_w_imm8);
	push_mcycle(gb, ld_a_wz);
}

// ------------------- LD hl, sp + imm8 ------------------

static void ld_hl_spz (GB *gb) {		// 0xF8
	int8_t v = gb->cpu.z;
	uint16_t r = gb->cpu.sp + v;
	gb->cpu.hl = r;
	set_z(gb, 0);
	set_n(gb, 0);
	uint8_t vu = (uint8_t) v;
	set_h(gb, ((gb->cpu.sp & 0xF) + (vu & 0xF)) & 0x10);
	set_c(gb, ((gb->cpu.sp & 0xFF) + vu) & 0x100);
}
static void decode_ld_hl_spimm8 (GB *gb) {
	push_mcycle(gb, ld_z_imm8);
	push_mcycle(gb, ld_hl_spz);
}

// ------------------ LD sp, hl -------------------

static void ld_sp_hl (GB *gb) {			// 0xF9
	gb->cpu.sp = gb->cpu.hl;
}

static void decode_ld_sp_hl (GB *gb) {
	push_mcycle(gb, ld_sp_hl);
}

void init_load (OpcodeTable *t)
{
	t->main[0x01] = decode_ld_bc_imm16;
	t->main[0x02] = decode_ld_bcmem_a;
	t->main[0x06] = decode_ld_b_imm8;
	t->main[0x08] = decode_ld_imm16_sp;
	t->main[0x0A] = decode_ld_a_bcmem;
	t->main[0x0E] = decode_ld_c_imm8;
	t->main[0x11] = decode_ld_de_imm16;
	t->main[0x12] = decode_ld_demem_a;
	t->main[0x16] = decode_ld_d_imm8;
	t->main[0x1A] = decode_ld_a_demem;
	t->main[0x1E] = decode_ld_e_imm8;
	t->main[0x21] = decode_ld_hl_imm16;
	t->main[0x22] = decode_ld_hlimem_a;
	t->main[0x26] = decode_ld_h_imm8;
	t->main[0x2A] = decode_ld_a_hlimem;
	t->main[0x2E] = decode_ld_l_imm8;
	t->main[0x31] = decode_ld_sp_imm16;
	t->main[0x32] = decode_ld_hldmem_a;
	t->main[0x36] = decode_ld_hl_imm8;
	t->main[0x3A] = decode_ld_a_hldmem;
	t->main[0x3E] = decode_ld_a_imm8;

	// -------------------- 0x40 - 0x7F (LD r,r) --------------------
	t->main[0x40] = ld_b_b;
	t->main[0x41] = ld_b_c;
	t->main[0x42] = ld_b_d;
	t->main[0x43] = ld_b_e;
	t->main[0x44] = ld_b_h;
	t->main[0x45] = ld_b_l;
	t->main[0x46] = decode_ld_b_hl;
	t->main[0x47] = ld_b_a;

	t->main[0x48] = ld_c_b;
	t->main[0x49] = ld_c_c;
	t->main[0x4A] = ld_c_d;
	t->main[0x4B] = ld_c_e;
	t->main[0x4C] = ld_c_h;
	t->main[0x4D] = ld_c_l;
	t->main[0x4E] = decode_ld_c_hl;
	t->main[0x4F] = ld_c_a;

	t->main[0x50] = ld_d_b;
	t->main[0x51] = ld_d_c;
	t->main[0x52] = ld_d_d;
	t->main[0x53] = ld_d_e;
	t->main[0x54] = ld_d_h;
	t->main[0x55] = ld_d_l;
	t->main[0x56] = decode_ld_d_hl;
	t->main[0x57] = ld_d_a;

	t->main[0x58] = ld_e_b;
	t->main[0x59] = ld_e_c;
	t->main[0x5A] = ld_e_d;
	t->main[0x5B] = ld_e_e;
	t->main[0x5C] = ld_e_h;
	t->main[0x5D] = ld_e_l;
	t->main[0x5E] = decode_ld_e_hl;
	t->main[0x5F] = ld_e_a;

	t->main[0x60] = ld_h_b;
	t->main[0x61] = ld_h_c;
	t->main[0x62] = ld_h_d;
	t->main[0x63] = ld_h_e;
	t->main[0x64] = ld_h_h;
	t->main[0x65] = ld_h_l;
	t->main[0x66] = decode_ld_h_hl;
	t->main[0x67] = ld_h_a;

	t->main[0x68] = ld_l_b;
	t->main[0x69] = ld_l_c;
	t->main[0x6A] = ld_l_d;
	t->main[0x6B] = ld_l_e;
	t->main[0x6C] = ld_l_h;
	t->main[0x6D] = ld_l_l;
	t->main[0x6E] = decode_ld_l_hl;
	t->main[0x6F] = ld_l_a;

	t->main[0x70] = decode_ld_hl_b;
	t->main[0x71] = decode_ld_hl_c;
	t->main[0x72] = decode_ld_hl_d;
	t->main[0x73] = decode_ld_hl_e;
	t->main[0x74] = decode_ld_hl_h;
	t->main[0x75] = decode_ld_hl_l;
	t->main[0x77] = decode_ld_hl_a;

	t->main[0x78] = ld_a_b;
	t->main[0x79] = ld_a_c;
	t->main[0x7A] = ld_a_d;
	t->main[0x7B] = ld_a_e;
	t->main[0x7C] = ld_a_h;
	t->main[0x7D] = ld_a_l;
	t->main[0x7E] = decode_ld_a_hl;
	t->main[0x7F] = ld_a_a;

	t->main[0xEA] = decode_ld_imm16_a;
	t->main[0xF8] = decode_ld_hl_spimm8;
	t->main[0xF9] = decode_ld_sp_hl;
	t->main[0xFA] = decode_ld_a_imm16;

	t->main[0xE0] = decode_ldh_imm8_a;
	t->main[0xE2] = decode_ldh_c_a;
	t->main[0xF0] = decode_ldh_a_imm8;
	t->main[0xF2] = decode_ldh_a_c;
}




















