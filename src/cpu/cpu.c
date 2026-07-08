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

static int handle_interrupts (CPU *cpu) {

	Bus *bus = cpu->bus;
	Interrupts *in = bus->interrupts;
	uint8_t fired = in->IE & in->IF & 0x1F;

	if (fired && cpu->halted)
		cpu->halted = 0;

	if (!in->IME || !fired)
		return 0;

	in->IME = 0;
	service(cpu);
	return 1;
}

void cpu_step (CPU *cpu) {

	GB *gb = (GB *) cpu->bus->ctx;

	if (cpu->instr_head >= cpu->instr_tail) {

		cpu->instr_tail = cpu->instr_head = 0;

		if (handle_interrupts(cpu)) {
			cpu->instr_stack[cpu->instr_head++](gb);
			return;
		}
		if (cpu->halted) return;

		if (cpu->bus->interrupts->ei_pending) {
			cpu->bus->interrupts->ei_pending = 0;
			cpu->bus->interrupts->IME = 1;
		}

		uint8_t opcode;
		if (cpu->halt_bug) {
			oam_bug((GB*)cpu->bus->ctx, cpu->pc, 0);
			opcode = cpu->bus->read8(cpu->bus->ctx, cpu->pc);
			cpu->halt_bug = 0;
		} else {
			oam_bug((GB*)cpu->bus->ctx, cpu->pc, 2);
			opcode = cpu->bus->read8(cpu->bus->ctx, cpu->pc++);
		}

		//if (opcode == 0xFB) cpu->bus->interrupts->ei_pending = 1;

		decode_instr(gb, opcode);

		return;
	}

	cpu->instr_stack[cpu->instr_head++](gb);
}
