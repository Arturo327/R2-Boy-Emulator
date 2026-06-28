#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "cpu/opcodes.h"
#include "bus/bus.h"

typedef struct GB GB;

typedef struct CPU {

	union {
		uint16_t af;
		struct {
			uint8_t f;
			uint8_t a;
		};
	};
	union {
		uint16_t bc;
		struct {
			uint8_t c;
			uint8_t b;
		};
	};
	union {
		uint16_t de;
		struct {
			uint8_t e;
			uint8_t d;
		};
	};
	union {
		uint16_t hl;
		struct {
			uint8_t l;
			uint8_t h;
		};
	};
	uint16_t sp;
	uint16_t pc;

	union {
		uint16_t wz;
		struct {
			uint8_t z;
			uint8_t w;
		};
	};

	uint8_t halted;
	uint8_t halt_bug;

	int8_t instr_head;
	int8_t instr_tail;
	opcode_fn instr_stack[6];

	Bus *bus;
} CPU;

void cpu_step (CPU *cpu);
void init_cpu (CPU *cpu);

#endif
