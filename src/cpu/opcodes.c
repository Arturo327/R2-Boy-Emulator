#include "gb.h"
#include "cpu/opcodes.h"
#include <stdlib.h>

#define FLAG_Z 0x80
#define FLAG_N 0x40
#define FLAG_H 0x20
#define FLAG_C 0x10

// ----------------- HELPERS ----------------

#define DECODE8(fn)		\
void decode_##fn (GB *gb) {	\
	push_mcycle(gb, fn);	\
}

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

static void push_mcycle (GB *gb, opcode_fn fn) {
	gb->cpu.instr_stack[gb->cpu.instr_tail++] = fn;
}

static inline void clear_instrs (GB *gb) {
	gb->cpu.instr_head = gb->cpu.instr_tail = 0;
}

// -------------- MEM ACCESS ----------------

void ld_hl_buff (GB *gb) {
	write8(gb, gb->cpu.hl, gb->cpu.buff);
}

void ld_buff_hl (GB *gb) {
	gb->cpu.buff = read8(gb, gb->cpu.hl);
}

void ld_buff_imm8 (GB *gb) {
	gb->cpu.buff = read8(gb, gb->cpu.pc++);
}

void ld_buff_h_imm8 (GB *gb) {
	gb->cpu.buff_h = read8(gb, gb->cpu.pc++);
}

void ld_sp_low_imm8 (GB *gb) {
	gb->cpu.sp = (uint16_t)read8(gb, gb->cpu.pc++);
}

void ld_sp_high_imm8 (GB *gb) {
	gb->cpu.sp |= ((uint16_t)read8(gb, gb->cpu.pc++)) << 8;
}

void ld_buffmem_sp_low (GB *gb) {
	uint8_t sp_l = (uint8_t)gb->cpu.sp;
	write8(gb, gb->cpu.buff16, sp_l);
}

void ld_buffmem_sp_high (GB *gb) {
	uint8_t sp_h = (uint8_t)(gb->cpu.sp >> 8);
	write8(gb, gb->cpu.buff16 + 1, sp_h);
}

// ---------------------- BLOCK 0 --------------------------

// ---------------------- NOP --------------------------

void nop (GB *gb) { return; }

// --------------------- STOP ------------------------

void stop (GB *gb) {				// 0x10
	gb->cpu.pc++;
	gb->cpu.halted = 1;
}

// ------------------------------- ld [r16mem], a ------------------------------------

void ld_bcmem_a (GB *gb) {			// 0x02
	write8(gb, gb->cpu.bc, gb->cpu.a);
}
DECODE8(ld_bcmem_a)

void ld_demem_a (GB *gb) {			// 0x12
	write8(gb, gb->cpu.de, gb->cpu.a);
}
DECODE8(ld_demem_a)

void ld_hlimem_a (GB *gb) {			// 0x22
	write8(gb, gb->cpu.hl, gb->cpu.a);
	gb->cpu.hl++;
}
DECODE8(ld_hlimem_a)

void ld_hldmem_a (GB *gb) {			// 0x32
	write8(gb, gb->cpu.hl, gb->cpu.a);
	gb->cpu.hl--;
}
DECODE8(ld_hldmem_a)

// ------------------- ld [imm16], sp ------------------

void decode_ld_imm16_sp (GB *gb) {	// 0x08
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_buff_h_imm8);
	push_mcycle(gb, ld_buffmem_sp_low);
	push_mcycle(gb, ld_buffmem_sp_high);
}

// ------------------ ld r8, imm8 ---------------------

#define DEF_LD_NEXT8(name)			\
void ld_##name##_imm8 (GB *gb) {		\
	gb->cpu.name = read8(gb, gb->cpu.pc++);	\
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
void decode_ld_##name##_imm8 (GB *gb) {		\
	push_mcycle(gb, ld_##name##_imm8);	\
}

DEF_DECODE_LD_NEXT8(b)
DEF_DECODE_LD_NEXT8(c)
DEF_DECODE_LD_NEXT8(d)
DEF_DECODE_LD_NEXT8(e)
DEF_DECODE_LD_NEXT8(h)
DEF_DECODE_LD_NEXT8(l)
DEF_DECODE_LD_NEXT8(a)

void decode_ld_hl_imm8 (GB *gb) {	// 0x36
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_hl_buff);
}

#undef DEF_DECODE_LD_NEXT8

// --------------------------- ld r16, imm16 ---------------------

void decode_ld_bc_imm16 (GB *gb) {
	push_mcycle(gb, ld_c_imm8);
	push_mcycle(gb, ld_b_imm8);
}
void decode_ld_de_imm16 (GB *gb) {
	push_mcycle(gb, ld_e_imm8);
	push_mcycle(gb, ld_d_imm8);
}
void decode_ld_hl_imm16 (GB *gb) {
	push_mcycle(gb, ld_l_imm8);
	push_mcycle(gb, ld_h_imm8);
}

void decode_ld_sp_imm16 (GB *gb) {
	push_mcycle(gb, ld_sp_low_imm8);
	push_mcycle(gb, ld_sp_high_imm8);
}

// --------------------------------- ld a, [r16mem] ----------------------------

void ld_a_bcmem (GB *gb) {			// 0x0A
	gb->cpu.a = read8(gb, gb->cpu.bc);
}
DECODE8(ld_a_bcmem)

void ld_a_demem (GB *gb) {			// 0x1A
	gb->cpu.a = read8(gb, gb->cpu.de);
}
DECODE8(ld_a_demem)

void ld_a_hlimem (GB *gb) {			// 0x2A
	gb->cpu.a = gb->cpu.bus->read8(gb->cpu.bus->ctx, gb->cpu.hl);
	gb->cpu.hl++;
	oam_bug(gb, gb->cpu.hl, 2);
}
DECODE8(ld_a_hlimem)

void ld_a_hldmem (GB *gb) {			// 0x3A
	gb->cpu.a = gb->cpu.bus->read8(gb->cpu.bus->ctx, gb->cpu.hl);
	gb->cpu.hl--;
	oam_bug(gb, gb->cpu.hl, 2);
}
DECODE8(ld_a_hldmem)

// ---------------------------------- inc r16 ----------------------------------

#define DEF_INC(name)			\
void inc16_##name (GB *gb) {		\
	gb->cpu.name++;			\
	oam_bug(gb, gb->cpu.name, 1);	\
}

DEF_INC(bc)		// 0x03
DEF_INC(de)		// 0x13
DEF_INC(hl)		// 0x23
DEF_INC(sp)		// 0x33

#undef DEF_INC

#define DEF_DECODE_INC(name)		\
void decode_inc16_##name (GB *gb) {	\
	push_mcycle(gb, inc16_##name);	\
}

DEF_DECODE_INC(bc)
DEF_DECODE_INC(de)
DEF_DECODE_INC(hl)
DEF_DECODE_INC(sp)

#undef DEF_DECODE_INC

// ------------------------------- dec r16 -------------------------------

#define DEF_DEC16(name)			\
void dec16_##name (GB *gb) {		\
	gb->cpu.name--;			\
	oam_bug(gb, gb->cpu.name, 1);	\
}

DEF_DEC16(bc)		// 0x0B
DEF_DEC16(de)		// 0x1B
DEF_DEC16(hl)		// 0x2B
DEF_DEC16(sp)		// 0x3B

#undef DEF_DEC16

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
void add_hl_##name (GB *gb) {					\
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
void decode_add_hl_##name (GB *gb) {		\
	push_mcycle(gb, add_hl_##name);	\
}

DEF_DECODE_ADD_HL_R16(bc)
DEF_DECODE_ADD_HL_R16(de)
DEF_DECODE_ADD_HL_R16(hl)
DEF_DECODE_ADD_HL_R16(sp)

#undef DEF_DECODE_ADD_HL_R16

// -------------------------- inc r8 --------------------------

#define DEF_INC(name)			\
void inc8_##name (GB *gb) {		\
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

void inc8_hl (GB *gb) {		// 0x34
	uint8_t hl = read8(gb, gb->cpu.hl);
	gb->cpu.buff = hl + 1;
	set_z(gb, gb->cpu.buff == 0);
	set_n(gb, 0);
	set_h(gb, (hl & 0xF) == 0xF);
}

void decode_inc8_hl (GB *gb) {
	push_mcycle(gb, inc8_hl);
	push_mcycle(gb, ld_hl_buff);
}

// ------------------------ dec r8 --------------------------

#define DEF_DEC(name)			\
void dec8_##name (GB *gb) {		\
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

void dec8_hl (GB *gb) {		// 0x35
	uint8_t hl = read8(gb, gb->cpu.hl);
	gb->cpu.buff = hl - 1;
	set_z(gb, gb->cpu.buff == 0);
	set_n(gb, 1);
	set_h(gb, (hl & 0xF) == 0);
}

void decode_dec8_hl (GB *gb) {
	push_mcycle(gb, dec8_hl);
	push_mcycle(gb, ld_hl_buff);
}

// --------------------- jr imm8 --------------------

void jr_buff (GB *gb) {
	gb->cpu.pc += (int8_t)gb->cpu.buff;
}

void decode_jr_imm8 (GB *gb) {		// 0x18
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, jr_buff);
}

// ------------------- jr cond, imm8 ------------------

void jr_check_z (GB *gb) {
	uint8_t a8 = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) {
		gb->cpu.buff = a8;
		return;
	}
	clear_instrs(gb);
}
void decode_jr_z_imm8 (GB *gb) {	// 0x28
	push_mcycle(gb, jr_check_z);
	push_mcycle(gb, jr_buff);
}

void jr_check_c (GB *gb) {
	uint8_t a8 = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) {
		gb->cpu.buff = a8;
		return;
	}
	clear_instrs(gb);
}
void decode_jr_c_imm8 (GB *gb) {	// 0x38
	push_mcycle(gb, jr_check_c);
	push_mcycle(gb, jr_buff);
}

void jr_check_nz (GB *gb) {
	uint8_t a8 = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) {
		clear_instrs(gb);
		return;
	}
	gb->cpu.buff = a8;
}
void decode_jr_nz_imm8 (GB *gb) {	// 0x20
	push_mcycle(gb, jr_check_nz);
	push_mcycle(gb, jr_buff);
}

void jr_check_nc (GB *gb) {
	uint8_t a8 = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) {
		clear_instrs(gb);
		return;
	}
	gb->cpu.buff = a8;
}
void decode_jr_nc_imm8 (GB *gb) {	// 0x30
	push_mcycle(gb, jr_check_nc);
	push_mcycle(gb, jr_buff);
}

// ---------------------- RLCA --------------------

void rlca (GB *gb) {				// 0x07
	uint8_t bit = gb->cpu.a >> 7;
	gb->cpu.a = (gb->cpu.a << 1) | bit;

	set_c(gb, bit != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// -------------------- RLA ----------------------

void rla (GB *gb) {				// 0x17
	uint8_t new_carry = gb->cpu.a >> 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) >> 4;
	gb->cpu.a = (gb->cpu.a << 1) | old_carry;

	set_c(gb, new_carry != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// ---------------------- RRCA --------------------

void rrca (GB *gb) {				// 0x0F
	uint8_t bit = gb->cpu.a << 7;
	gb->cpu.a = (gb->cpu.a >> 1) | bit;

	set_c(gb, bit != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// -------------------- RRA ----------------------

void rra (GB *gb) {				// 0x1F
	uint8_t new_carry = gb->cpu.a << 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) << 3;
	gb->cpu.a = (gb->cpu.a >> 1) | old_carry;

	set_c(gb, new_carry != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// ------------------- DAA ---------------------

void daa (GB *gb) {				// 0x27
	uint8_t a = gb->cpu.a;

	uint8_t adjust = 0;
	uint8_t carry = 0;

	if (gb->cpu.f & FLAG_N) {
		if (gb->cpu.f & FLAG_H) {
			adjust |= 0x06;
		}
		if (gb->cpu.f & FLAG_C) {
			carry = 1;
			adjust |= 0x60;
		}

		a -= adjust;
	} else {
		if ((gb->cpu.f & FLAG_H) || (a & 0x0F) > 9) {
			adjust |= 0x06;
		}
		if ((gb->cpu.f & FLAG_C) || a > 0x99) {
			adjust |= 0x60;
			carry = 1;
		}

		a += adjust;
	}

	gb->cpu.a = a;

	set_z(gb, a == 0);
	set_h(gb, 0);
	set_c(gb, carry);

}

// -------------------- SCF -------------------------

void scf (GB *gb) {			// 0x37
	set_h(gb, 0);
	set_n(gb, 0);
	set_c(gb, 1);
}

// ------------------- CPL ----------------------

void cpl (GB *gb) {			// 0x2F
	gb->cpu.a = ~gb->cpu.a;
	set_n(gb, 1);
	set_h(gb, 1);
}

// -------------------- CCF ---------------------

void ccf (GB *gb) {			// 0x3F
	gb->cpu.f ^= FLAG_C;
	set_n(gb, 0);
	set_h(gb, 0);
}

// ---------------------- BLOCK 1 ---- 8-bit register-to-register loads ------------------

// --------------------- HALT -------------------------

void halt (GB *gb) {				// 0x76
	uint8_t fired = gb->interrupts.IE & gb->interrupts.IF & 0x1F;
	if (!gb->interrupts.IME && fired) {
		gb->cpu.halt_bug = 1;
		return;
	}
	gb->cpu.halted = 1;
}

// -------------------- LD r8, r8 ----------------------

#define DEF_LD(dest, src)		\
void ld_##dest##_##src (GB *gb) {	\
	gb->cpu.dest = gb->cpu.src;	\
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
void ld_##name##_hl (GB *gb) {			\
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
void decode_ld_##name##_hl (GB *gb) {		\
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
void ld_hl_##name (GB *gb) {			\
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

#define DECODE_DEF_LD_HL(name)		\
void decode_ld_hl_##name (GB *gb) {	\
	push_mcycle(gb, ld_hl_##name);\
}

DECODE_DEF_LD_HL(b)	// 0x70
DECODE_DEF_LD_HL(c)	// 0x71
DECODE_DEF_LD_HL(d)	// 0x72
DECODE_DEF_LD_HL(e)	// 0x73
DECODE_DEF_LD_HL(h)	// 0x74
DECODE_DEF_LD_HL(l)	// 0x75
DECODE_DEF_LD_HL(a)	// 0x77

#undef DECODE_DEF_LD_HL

// ----------------------- BLOCK 2 ---- 8-bit arithmetic -------------------

// --------------------- ADD a, r8 --------------------

#define DEF_ADD(name)						\
void add_a_##name (GB *gb) {					\
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

void add_a_hl (GB *gb) {	// 0x86
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
void adc_a_##name (GB *gb) {				\
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

void adc_a_hl (GB *gb) {		// 0x8E
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
void sub_a_##name (GB *gb) {			\
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

void sub_a_hl (GB *gb) {		// 0x96
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
void sbc_a_##name (GB *gb) {				\
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

void sbc_a_hl (GB *gb) {
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
void and_a_##name (GB *gb) {		\
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

void and_a_hl (GB *gb) {		// 0xA6
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
void xor_a_##name (GB *gb) {		\
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

void xor_a_hl (GB *gb) {		// 0xAE
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
void or_a_##name (GB *gb) {		\
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

void or_a_hl (GB *gb) {		// 0xB6
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
void cp_a_##name (GB *gb) {					\
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

void cp_a_hl (GB *gb) {		// 0xBE
	uint8_t v = read8(gb, gb->cpu.hl);
	uint8_t tmp = gb->cpu.a - v;	
	set_z(gb, tmp == 0);
	set_n(gb, 1);
	set_h(gb, (gb->cpu.a & 0x0F) < (v & 0x0F));
	set_c(gb, gb->cpu.a < v);
}
DECODE8(cp_a_hl)

#undef DEF_CP

// ---------------------- BLOCK 3 ----------------------

// ---------------------- ADD a, imm8 ------------------

void add_a_imm8 (GB *gb) {		// 0xC6
	uint8_t old = gb->cpu.a;
	gb->cpu.a += read8(gb, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old & 0x0F) > (gb->cpu.a & 0x0F));
	set_c(gb, old > gb->cpu.a);
}
DECODE8(add_a_imm8)

// -------------------- ADC a, imm8 -------------------

void adc_a_imm8 (GB *gb) {			// 0xCE
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t v = read8(gb, gb->cpu.pc++);
	uint16_t r = (uint16_t)old + v + c;
	gb->cpu.a = (uint8_t)r;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, (old ^ r ^ v) & 0x10);
	set_c(gb, r > 0xFF);
}
DECODE8(adc_a_imm8)

// ------------------- SUB a, imm8 -------------------

void sub_a_imm8 (GB *gb) {			// 0xD6
	uint8_t old = gb->cpu.a;
	uint8_t v = read8(gb, gb->cpu.pc++);
	gb->cpu.a -= v;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F));
	set_c(gb, old < v);
}
DECODE8(sub_a_imm8)

// ------------------- SBC a, imm8 ------------------

void sbc_a_imm8 (GB *gb) {			// 0xDE
	uint8_t old = gb->cpu.a;
	uint8_t c = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t v = read8(gb, gb->cpu.pc++);
	gb->cpu.a = old - v - c;
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 1);
	set_h(gb, (old & 0x0F) < (v & 0x0F) + c);
	set_c(gb, (uint16_t)old < (uint16_t)v + c);
}
DECODE8(sbc_a_imm8)

// ------------------ AND a, imm8 ------------------

void and_a_imm8 (GB *gb) {			// 0xE6
	gb->cpu.a &= read8(gb, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 1);
	set_c(gb, 0);
}
DECODE8(and_a_imm8)

// ------------------ XOR a, imm8 -------------------

void xor_a_imm8 (GB *gb) {			// 0xEE
	gb->cpu.a ^= read8(gb, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(xor_a_imm8)

// ------------------- OR a, imm8 ------------------

void or_a_imm8 (GB *gb) {			// 0xF6
	gb->cpu.a |= read8(gb, gb->cpu.pc++);
	set_z(gb, gb->cpu.a == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}
DECODE8(or_a_imm8)

// ------------------- CP a, imm8 ------------------

void cp_a_imm8 (GB *gb) {			// 0xFE
	uint8_t v = read8(gb, gb->cpu.pc++);
	uint8_t tmp = gb->cpu.a - v;	
	set_z(gb, tmp == 0);
	set_n(gb, 1);
	set_h(gb, (gb->cpu.a & 0x0F) < (v & 0x0F));
	set_c(gb, gb->cpu.a < v);
}
DECODE8(cp_a_imm8)

// -------------------- HELPERS -------------------

void pop_l_buff (GB *gb) {
	gb->cpu.buff = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
void pop_h_buff (GB *gb) {
	gb->cpu.buff_h = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 0);
}
void ret (GB *gb) {
	gb->cpu.pc = gb->cpu.buff16;
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
	gb->cpu.buff_h = read8(gb, gb->cpu.pc++);
	if (!(gb->cpu.f & FLAG_Z)) clear_instrs(gb);
}
void check_jp_c (GB *gb) {
	gb->cpu.buff_h = read8(gb, gb->cpu.pc++);
	if (!(gb->cpu.f & FLAG_C)) clear_instrs(gb);
}
void check_jp_nz (GB *gb) {
	gb->cpu.buff_h = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_Z) clear_instrs(gb);
}
void check_jp_nc (GB *gb) {
	gb->cpu.buff_h = read8(gb, gb->cpu.pc++);
	if (gb->cpu.f & FLAG_C) clear_instrs(gb);
}

void jump (GB *gb) {
	gb->cpu.pc = gb->cpu.buff16;
}

// ------------------ RET ------------------------

void decode_ret (GB *gb) {
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, ret);
}

// ------------------ RET cond ------------------------

void decode_ret_z (GB *gb) {			// 0xC9
	push_mcycle(gb, check_ret_z);
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, ret);
}

void decode_ret_c (GB *gb) {			// 0xD8
	push_mcycle(gb, check_ret_c);
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, ret);
}

void decode_ret_nz (GB *gb) {			// 0xC0
	push_mcycle(gb, check_ret_nz);
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, ret);
}

void decode_ret_nc (GB *gb) {			// 0xD0
	push_mcycle(gb, check_ret_nc);
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, ret);
}

// ------------------ RETI ------------------------

void reti (GB *gb) {				// 0xD9
	gb->cpu.pc = gb->cpu.buff16;
	gb->cpu.bus->interrupts->IME = 1;
}
void decode_reti (GB *gb) {
	push_mcycle(gb, pop_l_buff);
	push_mcycle(gb, pop_h_buff);
	push_mcycle(gb, reti);
}

// ------------------- JP imm16 -----------------------

void decode_jp_imm16 (GB *gb) {				// 0xC3
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_buff_h_imm8);
	push_mcycle(gb, jump);
}

// ------------------ JP cond, imm16 ------------------------

void decode_jp_z_imm16 (GB *gb) {		// 0xCA
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_z);
	push_mcycle(gb, jump);
}

void decode_jp_c_imm16 (GB *gb) {		// 0xDA
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_c);
	push_mcycle(gb, jump);
}

void decode_jp_nz_imm16 (GB *gb) {		// 0xC2
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_nz);
	push_mcycle(gb, jump);
}

void decode_jp_nc_imm16 (GB *gb) {		// 0xD2
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_nc);
	push_mcycle(gb, jump);
}

// --------------------- JP hl -----------------------

void jp_hl (GB *gb) {				// 0xE9
	gb->cpu.pc = gb->cpu.hl;
}

// --------------------- CALL imm16 ------------------

void push_pc_h (GB *gb) {
	write8(gb, --gb->cpu.sp, (uint8_t)(gb->cpu.pc >> 8));
	oam_bug(gb, gb->cpu.sp, 1);
}
void push_pc_l (GB *gb) {
	write8(gb, gb->cpu.sp, (uint8_t)gb->cpu.pc);
	oam_bug(gb, gb->cpu.sp, 1);
}

void call_buff (GB *gb) {
	push_pc_l(gb);
	jump(gb);
}

void decode_call_imm16 (GB *gb) {		// 0xCD
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_buff_h_imm8);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

void service (CPU *cpu, uint16_t addr, uint8_t flag_bit) {
	cpu->bus->interrupts->IF &= ~flag_bit;
	GB *gb = (GB *)cpu->bus->ctx;
	gb->cpu.buff16 = addr;
	push_mcycle(gb, nop);
	push_mcycle(gb, nop);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

// --------------------- CALL cond, imm16 ------------------

void decode_call_z_imm16 (GB *gb) {		// 0xCC
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_z);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

void decode_call_c_imm16 (GB *gb) {		// 0xDC
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_c);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

void decode_call_nz_imm16 (GB *gb) {		// 0xC4
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_nz);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

void decode_call_nc_imm16 (GB *gb) {		// 0xD4
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, check_jp_nc);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, call_buff);
}

// -------------------- RST tgt3 -------------------------

#define DEF_RST(num)		\
void rst_##num (GB *gb) {	\
	push_pc_l(gb);		\
	gb->cpu.pc = num << 3;	\
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
void decode_rst_##num (GB *gb) {	\
	push_mcycle(gb, push_pc_h);	\
	push_mcycle(gb, dec16_sp);	\
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

// --------------------- POP r16 -----------------------

void pop_bc_l (GB *gb) {
	gb->cpu.c = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
void pop_de_l (GB *gb) {
	gb->cpu.e = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
void pop_hl_l (GB *gb) {
	gb->cpu.l = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
void pop_af_l (GB *gb) {
	gb->cpu.f = read8(gb, gb->cpu.sp++) & 0xF0;
	oam_bug(gb, gb->cpu.sp, 2);
}

void pop_bc_h (GB *gb) {
	gb->cpu.b = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 0);
}
void pop_de_h (GB *gb) {
	gb->cpu.d = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 0);
}
void pop_hl_h (GB *gb) {
	gb->cpu.h = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 0);
}
void pop_af_h (GB *gb) {
	gb->cpu.a = read8(gb, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 0);
}

#define DEF_POP(name)				\
void decode_pop_##name (GB *gb) {		\
	push_mcycle(gb, pop_##name##_l);	\
	push_mcycle(gb, pop_##name##_h);	\
}

DEF_POP(bc)	// 0xC1
DEF_POP(de)	// 0xD1
DEF_POP(hl)	// 0xE1
DEF_POP(af)	// 0xF1
 
#undef DEF_POP

// --------------------- PUSH r16 -----------------------

void push_bc_l (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.c);
	oam_bug(gb, gb->cpu.sp, 1);
}
void push_de_l (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.e);
	oam_bug(gb, gb->cpu.sp, 1);
}
void push_hl_l (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.l);
	oam_bug(gb, gb->cpu.sp, 1);
}
void push_af_l (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.f);
	oam_bug(gb, gb->cpu.sp, 1);
}

void push_bc_h (GB *gb) {
	write8(gb, --gb->cpu.sp, gb->cpu.b);
	oam_bug(gb, gb->cpu.sp, 2);
}
void push_de_h (GB *gb) {
	write8(gb, --gb->cpu.sp, gb->cpu.d);
	oam_bug(gb, gb->cpu.sp, 2);
}
void push_hl_h (GB *gb) {
	write8(gb, --gb->cpu.sp, gb->cpu.h);
	oam_bug(gb, gb->cpu.sp, 2);
}
void push_af_h (GB *gb) {
	write8(gb, --gb->cpu.sp, gb->cpu.a);
	oam_bug(gb, gb->cpu.sp, 2);
}

#define DEF_PUSH(name)				\
void decode_push_##name (GB *gb) {		\
	push_mcycle(gb, push_##name##_h);	\
	push_mcycle(gb, dec16_sp);		\
	push_mcycle(gb, push_##name##_l);	\
}

DEF_PUSH(bc)	// 0xC5
DEF_PUSH(de)	// 0xD5
DEF_PUSH(hl)	// 0xE5
DEF_PUSH(af)	// 0xF5
 
#undef DEF_PUSH

// ---------------------- LDH [c], a -----------------------

void ldh_c_a (GB *gb) {			// 0xE2
	write8(gb, gb->cpu.c + 0xFF00, gb->cpu.a);
}
DECODE8(ldh_c_a)

// ---------------------- LDH a, [c] -----------------------

void ldh_a_c (GB *gb) {			// 0xF2
	gb->cpu.a = read8(gb, gb->cpu.c + 0xFF00);
}
DECODE8(ldh_a_c)

// --------------------- LDH [imm8], a ---------------------

void ldh_buff_a (GB *gb) {		// 0xE0
	write8(gb, 0xFF00 + gb->cpu.buff, gb->cpu.a);
}
void decode_ldh_imm8_a (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ldh_buff_a);
}

// -------------------- LDH a, [imm8] ----------------------

void ldh_a_buff (GB *gb) {		// 0xF0
	gb->cpu.a = read8(gb, 0xFF00 + gb->cpu.buff);
}
void decode_ldh_a_imm8 (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ldh_a_buff);
}

// ------------------- LD [imm16], a --------------------

void ld_buff16_a (GB *gb) {		// 0xEA
	write8(gb, gb->cpu.buff16, gb->cpu.a);
}
void decode_ld_imm16_a (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_buff_h_imm8);
	push_mcycle(gb, ld_buff16_a);
}

// ------------------- LD a, [imm16] --------------------

void ld_a_buff16 (GB *gb) {		// 0xFA
	gb->cpu.a = read8(gb, gb->cpu.buff16);
}
void decode_ld_a_imm16 (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_buff_h_imm8);
	push_mcycle(gb, ld_a_buff16);
}

// ------------------- ADD sp, imm8 --------------------

void add_sp_buff (GB *gb) {		// 0xE8
	uint16_t old = gb->cpu.sp;
	int8_t v = (int8_t)gb->cpu.buff;
	gb->cpu.sp = old + v;
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, ((old & 0xF) + (v & 0xF)) > 0xF);
	set_c(gb, ((old & 0xFF) + (v & 0xFF)) > 0xFF);
}
void decode_add_sp_imm8 (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, nop);
	push_mcycle(gb, add_sp_buff);
}

// ------------------- LD hl, sp + imm8 ------------------

void ld_hl_spbuff (GB *gb) {		// 0xF8
	int8_t v = (int16_t)gb->cpu.buff;
	uint16_t r = gb->cpu.sp + v;
	gb->cpu.hl = r;
	set_z(gb, 0);
	set_n(gb, 0);
	uint8_t vu = (uint8_t) v;
	set_h(gb, ((gb->cpu.sp & 0xF) + (vu & 0xF)) & 0x10);
	set_c(gb, ((gb->cpu.sp & 0xFF) + vu) & 0x100);
}
void decode_ld_hl_spimm8 (GB *gb) {
	push_mcycle(gb, ld_buff_imm8);
	push_mcycle(gb, ld_hl_spbuff);
}

// ------------------ LD sp, hl -------------------

void ld_sp_hl (GB *gb) {			// 0xF9
	gb->cpu.sp = gb->cpu.hl;
}
void decode_ld_sp_hl (GB *gb) {
	push_mcycle(gb, ld_sp_hl);
}

// --------------- DI ----------------

void di (GB *gb) {			// 0xF3
	gb->interrupts.IME = 0;
}

// -------------- EI ----------------

void ei (GB *gb) {			// 0xFB
	gb->interrupts.ei_pending = 1;
}

// ----------------------- CB PREFIX INSTUCTIONS -----------------------

// ------------------ RLC r8 ------------------

#define DEF_RLC(name)					\
void rlc_##name (GB *gb) {				\
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

void rlc_hl (GB *gb) {			// 0x06
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl >> 7;
	uint8_t r = (hl << 1) | bit;
	gb->cpu.buff = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RLC

// ------------------ RRC r8 ------------------

#define DEF_RRC(name)					\
void rrc_##name (GB *gb) {				\
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

void rrc_hl (GB *gb) {			// 0x0E
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl << 7;
	uint8_t r = (hl >> 1) | bit;
	gb->cpu.buff = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RRC

// -------------------------- RL r8 --------------------

#define DEF_RL(name)					\
void rl_##name (GB *gb) {				\
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

void rl_hl (GB *gb) {			// 0x16
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t new_carry = hl >> 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) >> 4;
	uint8_t r = (hl << 1) | old_carry;

	gb->cpu.buff = r;
	set_c(gb, new_carry != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RL

// ----------------------- RR r8 --------------------

#define DEF_RR(name)					\
void rr_##name (GB *gb) {				\
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

void rr_hl (GB *gb) {			// 0x1E
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t new_carry = hl << 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) << 3;
	uint8_t r = (hl >> 1) | old_carry;

	gb->cpu.buff = r;
	set_c(gb, new_carry != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_RR

// -------------------- SLA r8 ------------------

#define DEF_SLA(name)				\
void sla_##name (GB *gb) {			\
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

void sla_hl (GB *gb) {			// 0x26
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t bit = hl & 0x80;
	uint8_t r = hl << 1;

	gb->cpu.buff = r;
	set_c(gb, bit != 0);
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_SLA

// -------------------- SRA r8 ------------------

#define DEF_SRA(name)				\
void sra_##name (GB *gb) {			\
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

void sra_hl (GB *gb) {			// 0x2E
	uint8_t hl = read8(gb, gb->cpu.hl);
	set_c(gb, hl & 1);
	uint8_t a = hl & 0x80;
	uint8_t r = (hl >> 1) | a;
	gb->cpu.buff = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

#undef DEF_SRA

// -------------------- SRL r8 ------------------

#define DEF_SRL(name)			\
void srl_##name (GB *gb) {		\
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

void srl_hl (GB *gb) {			// 0x3E
	uint8_t hl = read8(gb, gb->cpu.hl);
	set_c(gb, hl & 1);
	uint8_t r = hl >> 1;
	gb->cpu.buff = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
}


#undef DEF_SRL

// -------------------- SWAP r8 -------------------------

#define DEF_SWAP(name)			\
void swap_##name (GB *gb) {		\
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

void swap_hl (GB *gb) {		// 0x36
	uint8_t hl = read8(gb, gb->cpu.hl);
	uint8_t r = (hl >> 4) | (hl << 4);
	gb->cpu.buff = r;
	set_z(gb, r == 0);
	set_n(gb, 0);
	set_h(gb, 0);
	set_c(gb, 0);
}

#undef DEF_SWAP

// --------------------- BIT b3, r8 ------------------

#define DEF_BIT(idx, reg)			\
void bit_##idx##_##reg (GB *gb) {		\
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
void bit_##idx##_hl (GB *gb) {				\
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
void res_##idx##_##reg (GB *gb) {		\
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
void res_##idx##_hl (GB *gb) {			\
	uint8_t hl = read8(gb, gb->cpu.hl);	\
	gb->cpu.buff = hl & ~(1 << idx);	\
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

#define DEF_SET(idx, reg)		\
void set_##idx##_##reg (GB *gb) {	\
	gb->cpu.reg |= 1 << idx;	\
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
void set_##idx##_hl (GB *gb) {			\
	uint8_t hl = read8(gb, gb->cpu.hl);	\
	gb->cpu.buff = hl | (1 << idx);		\
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

// ------------------- DECODE ------------------

void fetch_cb (GB *gb) {
	uint8_t opcode = read8(gb, gb->cpu.pc++);
	if ((opcode & 7) == 6) {
		if (opcode >= 0x40 && opcode < 0x80) {
			push_mcycle(gb, gb->opcodes.cb[opcode]);
		} else {
			push_mcycle(gb, gb->opcodes.cb[opcode]);
			push_mcycle(gb, ld_hl_buff);
		}
	} else {
		gb->opcodes.cb[opcode](gb);
	}
}

void decode_instr (GB *gb, uint8_t opcode) {
	if (opcode == 0xCB) {
		gb->cpu.instr_stack[gb->cpu.instr_tail++] = fetch_cb;
		return;
	}
	gb->opcodes.main[opcode] (gb);
}

// --------------------- init -------------------------

void init_opcodes (OpcodeTable *t) {
	for (int i = 0; i < 256; i++) {
		t->main[i] = nop;
		t->cb[i] = nop;
	}
	t->main[0x00] = nop;
	t->main[0x01] = decode_ld_bc_imm16;
	t->main[0x02] = decode_ld_bcmem_a;
	t->main[0x03] = decode_inc16_bc;
	t->main[0x04] = inc8_b;
	t->main[0x05] = dec8_b;
	t->main[0x06] = decode_ld_b_imm8;
	t->main[0x07] = rlca;
	t->main[0x08] = decode_ld_imm16_sp;
	t->main[0x09] = decode_add_hl_bc;
	t->main[0x0A] = decode_ld_a_bcmem;
	t->main[0x0B] = decode_dec16_bc;
	t->main[0x0C] = inc8_c;
	t->main[0x0D] = dec8_c;
	t->main[0x0E] = decode_ld_c_imm8;
	t->main[0x0F] = rrca;

	// -------------------- 0x10 - 0x1F --------------------
	t->main[0x10] = stop;
	t->main[0x11] = decode_ld_de_imm16;
	t->main[0x12] = decode_ld_demem_a;
	t->main[0x13] = decode_inc16_de;
	t->main[0x14] = inc8_d;
	t->main[0x15] = dec8_d;
	t->main[0x16] = decode_ld_d_imm8;
	t->main[0x17] = rla;
	t->main[0x18] = decode_jr_imm8;
	t->main[0x19] = decode_add_hl_de;
	t->main[0x1A] = decode_ld_a_demem;
	t->main[0x1B] = decode_dec16_de;
	t->main[0x1C] = inc8_e;
	t->main[0x1D] = dec8_e;
	t->main[0x1E] = decode_ld_e_imm8;
	t->main[0x1F] = rra;

	// -------------------- 0x20 - 0x2F --------------------
	t->main[0x20] = decode_jr_nz_imm8;
	t->main[0x21] = decode_ld_hl_imm16;
	t->main[0x22] = decode_ld_hlimem_a;
	t->main[0x23] = decode_inc16_hl;
	t->main[0x24] = inc8_h;
	t->main[0x25] = dec8_h;
	t->main[0x26] = decode_ld_h_imm8;
	t->main[0x27] = daa;
	t->main[0x28] = decode_jr_z_imm8;
	t->main[0x29] = decode_add_hl_hl;
	t->main[0x2A] = decode_ld_a_hlimem;
	t->main[0x2B] = decode_dec16_hl;
	t->main[0x2C] = inc8_l;
	t->main[0x2D] = dec8_l;
	t->main[0x2E] = decode_ld_l_imm8;
	t->main[0x2F] = cpl;

	// -------------------- 0x30 - 0x3F --------------------
	t->main[0x30] = decode_jr_nc_imm8;
	t->main[0x31] = decode_ld_sp_imm16;
	t->main[0x32] = decode_ld_hldmem_a;
	t->main[0x33] = decode_inc16_sp;
	t->main[0x34] = decode_inc8_hl;
	t->main[0x35] = decode_dec8_hl;
	t->main[0x36] = decode_ld_hl_imm8;
	t->main[0x37] = scf;
	t->main[0x38] = decode_jr_c_imm8;
	t->main[0x39] = decode_add_hl_sp;
	t->main[0x3A] = decode_ld_a_hldmem;
	t->main[0x3B] = decode_dec16_sp;
	t->main[0x3C] = inc8_a;
	t->main[0x3D] = dec8_a;
	t->main[0x3E] = decode_ld_a_imm8;
	t->main[0x3F] = ccf;

	// -------------------- 0x40 - 0x7F (LD r,r + HALT) --------------------
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
	t->main[0x76] = halt;
	t->main[0x77] = decode_ld_hl_a;

	t->main[0x78] = ld_a_b;
	t->main[0x79] = ld_a_c;
	t->main[0x7A] = ld_a_d;
	t->main[0x7B] = ld_a_e;
	t->main[0x7C] = ld_a_h;
	t->main[0x7D] = ld_a_l;
	t->main[0x7E] = decode_ld_a_hl;
	t->main[0x7F] = ld_a_a;

	// -------------------- 0x80 - 0xBF (ALU) --------------------
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

	// -------------------- 0xC0 - 0xFF (control) --------------------
	t->main[0xC0] = decode_ret_nz;
	t->main[0xC1] = decode_pop_bc;
	t->main[0xC2] = decode_jp_nz_imm16;
	t->main[0xC3] = decode_jp_imm16;
	t->main[0xC4] = decode_call_nz_imm16;
	t->main[0xC5] = decode_push_bc;
	t->main[0xC6] = decode_add_a_imm8;
	t->main[0xC7] = decode_rst_0;

	t->main[0xC8] = decode_ret_z;
	t->main[0xC9] = decode_ret;
	t->main[0xCA] = decode_jp_z_imm16;
	t->main[0xCB] = fetch_cb;
	t->main[0xCC] = decode_call_z_imm16;
	t->main[0xCD] = decode_call_imm16;
	t->main[0xCE] = decode_adc_a_imm8;
	t->main[0xCF] = decode_rst_1;

	t->main[0xD0] = decode_ret_nc;
	t->main[0xD1] = decode_pop_de;
	t->main[0xD2] = decode_jp_nc_imm16;
	t->main[0xD3] = NULL;
	t->main[0xD4] = decode_call_nc_imm16;
	t->main[0xD5] = decode_push_de;
	t->main[0xD6] = decode_sub_a_imm8;
	t->main[0xD7] = decode_rst_2;

	t->main[0xD8] = decode_ret_c;
	t->main[0xD9] = decode_reti;
	t->main[0xDA] = decode_jp_c_imm16;
	t->main[0xDB] = NULL;
	t->main[0xDC] = decode_call_c_imm16;
	t->main[0xDD] = NULL;
	t->main[0xDE] = decode_sbc_a_imm8;
	t->main[0xDF] = decode_rst_3;

	t->main[0xE0] = decode_ldh_imm8_a;
	t->main[0xE1] = decode_pop_hl;
	t->main[0xE2] = decode_ldh_c_a;
	t->main[0xE3] = NULL;
	t->main[0xE4] = NULL;
	t->main[0xE5] = decode_push_hl;
	t->main[0xE6] = decode_and_a_imm8;
	t->main[0xE7] = decode_rst_4;

	t->main[0xE8] = decode_add_sp_imm8;
	t->main[0xE9] = jp_hl;
	t->main[0xEA] = decode_ld_imm16_a;
	t->main[0xEB] = NULL;
	t->main[0xEC] = NULL;
	t->main[0xED] = NULL;
	t->main[0xEE] = decode_xor_a_imm8;
	t->main[0xEF] = decode_rst_5;

	t->main[0xF0] = decode_ldh_a_imm8;
	t->main[0xF1] = decode_pop_af;
	t->main[0xF2] = decode_ldh_a_c;
	t->main[0xF3] = di;
	t->main[0xF4] = NULL;
	t->main[0xF5] = decode_push_af;
	t->main[0xF6] = decode_or_a_imm8;
	t->main[0xF7] = decode_rst_6;

	t->main[0xF8] = decode_ld_hl_spimm8;
	t->main[0xF9] = decode_ld_sp_hl;
	t->main[0xFA] = decode_ld_a_imm16;
	t->main[0xFB] = ei;
	t->main[0xFC] = NULL;
	t->main[0xFD] = NULL;
	t->main[0xFE] = decode_cp_a_imm8;
	t->main[0xFF] = decode_rst_7;

	// ==================== CB PREFIX TABLE ====================

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
