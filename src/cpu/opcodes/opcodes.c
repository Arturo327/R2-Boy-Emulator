#include "gb.h"
#include "cpu/opcodes/opcodes.h"
#include "cpu/opcodes/common.h"
#include <stdlib.h>
#include <stdio.h>

static void service_vector (GB *gb) {
	uint8_t fired = gb->interrupts.IE & gb->interrupts.IF & 0x1F;
	uint16_t addr = 0x0000;

	if (fired & 0x01) { addr = 0x40; gb->interrupts.IF &= ~0x01; }
	else if (fired & 0x02) { addr = 0x48; gb->interrupts.IF &= ~0x02; }
	else if (fired & 0x04) { addr = 0x50; gb->interrupts.IF &= ~0x04; }
	else if (fired & 0x08) { addr = 0x58; gb->interrupts.IF &= ~0x08; }
	else if (fired & 0x10) { addr = 0x60; gb->interrupts.IF &= ~0x10; }

	gb->cpu.wz = addr;
	push_pc_l(gb);
	jump(gb);
}

void service (CPU *cpu) {
	GB *gb = (GB *)cpu->bus->ctx;
	push_mcycle(gb, nop);
	push_mcycle(gb, nop);
	push_mcycle(gb, dec16_sp);
	push_mcycle(gb, push_pc_h);
	push_mcycle(gb, service_vector);
}

// ------------------- DECODE ------------------

void fetch_cb (GB *gb) {
	oam_bug(gb, gb->cpu.pc, 2);
	uint8_t opcode = gb->bus.read8(gb->bus.ctx, gb->cpu.pc++);
	if ((opcode & 7) == 6) {
		if (opcode >= 0x40 && opcode < 0x80) {
			push_mcycle(gb, gb->opcodes.cb[opcode]);
		} else {
			push_mcycle(gb, gb->opcodes.cb[opcode]);
			push_mcycle(gb, ld_hl_z);
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

void init_load (OpcodeTable *t);
void init_alu (OpcodeTable *t);
void init_cb (OpcodeTable *t);
void init_jump (OpcodeTable *t);
void init_stack (OpcodeTable *t);
void init_misc (OpcodeTable *t);

void init_opcodes (OpcodeTable *t)
{
	init_load(t);
	init_jump(t);
	init_alu(t);
	init_cb(t);
	init_stack(t);
	init_misc(t);
	t->main[0xCB] = fetch_cb;
}
