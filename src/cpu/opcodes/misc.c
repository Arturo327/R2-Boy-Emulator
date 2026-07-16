#include "cpu/opcodes/common.h"

// ------------------ ILLEGAL OPCODE ------------------

static void illegal_opcode (GB *gb)
{
	uint16_t pc = gb->cpu.pc - 1;
	uint8_t opcode = gb->bus.read8(gb->bus.ctx, pc);
	fprintf(stderr, "FATAL: Illegal opcode %02X at PC=%04X\n", opcode, pc);
	fprintf(stderr, "A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X SP=%04X\n",
		gb->cpu.a, gb->cpu.b, gb->cpu.c, gb->cpu.d,
		gb->cpu.e, gb->cpu.h, gb->cpu.l, gb->cpu.sp);
	gb->cpu.halted = 1;
	gb->running = 0;	
}

// --------------------- STOP ------------------------

static void stop (GB *gb) {				// 0x10
	gb->cpu.pc++;
	gb->cpu.halted = 1;
}

// ---------------------- RLCA --------------------

static void rlca (GB *gb) {				// 0x07
	uint8_t bit = gb->cpu.a >> 7;
	gb->cpu.a = (gb->cpu.a << 1) | bit;

	set_c(gb, bit != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// -------------------- RLA ----------------------

static void rla (GB *gb) {				// 0x17
	uint8_t new_carry = gb->cpu.a >> 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) >> 4;
	gb->cpu.a = (gb->cpu.a << 1) | old_carry;

	set_c(gb, new_carry != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// ---------------------- RRCA --------------------

static void rrca (GB *gb) {				// 0x0F
	uint8_t bit = gb->cpu.a << 7;
	gb->cpu.a = (gb->cpu.a >> 1) | bit;

	set_c(gb, bit != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// -------------------- RRA ----------------------

static void rra (GB *gb) {				// 0x1F
	uint8_t new_carry = gb->cpu.a << 7;
	uint8_t old_carry = (gb->cpu.f & FLAG_C) << 3;
	gb->cpu.a = (gb->cpu.a >> 1) | old_carry;

	set_c(gb, new_carry != 0);
	set_z(gb, 0);
	set_n(gb, 0);
	set_h(gb, 0);
}

// ------------------- DAA ---------------------

static void daa (GB *gb) {				// 0x27
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

static void scf (GB *gb) {			// 0x37
	set_h(gb, 0);
	set_n(gb, 0);
	set_c(gb, 1);
}

// ------------------- CPL ----------------------

static void cpl (GB *gb) {			// 0x2F
	gb->cpu.a = ~gb->cpu.a;
	set_n(gb, 1);
	set_h(gb, 1);
}

// -------------------- CCF ---------------------

static void ccf (GB *gb) {			// 0x3F
	gb->cpu.f ^= FLAG_C;
	set_n(gb, 0);
	set_h(gb, 0);
}

// --------------------- HALT -------------------------

static void halt (GB *gb) {			// 0x76
	uint8_t fired = gb->interrupts.IE & gb->interrupts.IF & 0x1F;
	if (!gb->interrupts.IME && fired) {
		gb->cpu.halt_bug = 1;
		return;
	}
	gb->cpu.halted = 1;
}

// --------------- DI ----------------

void di (GB *gb) {			// 0xF3
	gb->interrupts.IME = 0;
}

// -------------- EI ----------------

void ei (GB *gb) {			// 0xFB
	gb->interrupts.ei_pending = 1;
}

void init_misc (OpcodeTable *t)
{
	t->main[0x00] = nop;
	t->main[0x07] = rlca;
	t->main[0x0F] = rrca;
	t->main[0x10] = stop;
	t->main[0x17] = rla;
	t->main[0x1F] = rra;
	t->main[0x27] = daa;
	t->main[0x2F] = cpl;
	t->main[0x37] = scf;
	t->main[0x3F] = ccf;
	t->main[0x76] = halt;
	t->main[0xD3] = illegal_opcode;
	t->main[0xDB] = illegal_opcode;
	t->main[0xDD] = illegal_opcode;
	t->main[0xE4] = illegal_opcode;
	t->main[0xEB] = illegal_opcode;
	t->main[0xEC] = illegal_opcode;
	t->main[0xED] = illegal_opcode;
	t->main[0xF3] = di;
	t->main[0xF4] = illegal_opcode;
	t->main[0xFB] = ei;
	t->main[0xFC] = illegal_opcode;
	t->main[0xFD] = illegal_opcode;
}
