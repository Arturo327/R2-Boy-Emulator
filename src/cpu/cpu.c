#include "cpu/cpu.h"
#include <stdlib.h>

void init_cpu (CPU *cpu) {
	cpu->a = 0x01;
	cpu->f = 0xB0;
	cpu->b = 0x00;
	cpu->c = 0x13;
	cpu->d = 0x00;
	cpu->e = 0xD8;
	cpu->h = 0x01;
	cpu->l = 0x4D;

	cpu->sp = 0xFFFE;
	cpu->pc = 0x100;
	cpu->halted = 0;
	cpu->halt_bug = 0;

	cpu->bus = NULL;
}

static void service (CPU *cpu, uint16_t addr, uint8_t flag_bit) {
	cpu->bus->interrupts->IF &= ~flag_bit;
	cpu->sp -= 2;
	cpu->bus->write8(cpu->bus->ctx, cpu->sp, cpu->pc & 0xFF);
	cpu->bus->write8(cpu->bus->ctx, cpu->sp + 1, cpu->pc >> 8);
	cpu->pc = addr;
}

static int handle_interrupts (CPU *cpu) {

	Bus *bus = cpu->bus;
	Interrupts *in = bus->interrupts;
	uint8_t fired = in->IE & in->IF & 0x1F;

	if (fired && cpu->halted)
		cpu->halted = 0;

	if (!in->IME || !fired)
		return 0;

	in->IME = 0;
	if (fired & 0x01) service(cpu, 0x40, 0x01);
	else if (fired & 0x02) service(cpu, 0x48, 0x02);
	else if (fired & 0x04) service(cpu, 0x50, 0x04);
	else if (fired & 0x08) service(cpu, 0x58, 0x08);
	else if (fired & 0x10) service(cpu, 0x60, 0x10);

	return 1;
}

int cpu_step (CPU *cpu) {

	GB *gb = (GB *) cpu->bus->ctx;

	if (cpu->instr_head >= cpu->instr_tail) {
		if (handle_interrupts(cpu)) return 20;
		if (cpu->halted) return 4;

		cpu->instr_tail = cpu->instr_head = 0;

		if (cpu->bus->interrupts->ei_pending) {
			cpu->bus->interrupts->ei_pending = 0;
			cpu->bus->interrupts->IME = 1;
		}

		uint8_t opcode;
		if (cpu->halt_bug) {
			opcode = cpu->bus->read8(cpu->bus->ctx, cpu->pc);
			cpu->halt_bug = 0;
		} else {
			opcode = cpu->bus->read8(cpu->bus->ctx, cpu->pc++);
		}

		if (opcode == 0xFB) cpu->bus->interrupts->ei_pending = 1;

		decode_instr(gb, opcode);

		return 4;
	}

	cpu->instr_stack[cpu->instr_head++](gb);
	return 4;
}
