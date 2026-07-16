#ifndef OPCODES_H
#define OPCODES_H

#include <stdint.h>

typedef struct GB GB;
typedef struct CPU CPU;

typedef void (*opcode_fn) (GB *gb);

typedef struct {
	opcode_fn main[256];
	opcode_fn cb[256];
} OpcodeTable;

void service (CPU *cpu);
void decode_instr (GB *gb, uint8_t opcode);
void init_opcodes (OpcodeTable *t);

#endif
