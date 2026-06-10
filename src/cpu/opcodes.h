#ifndef OPCODES_H
#define OPCODES_H

#include <stdint.h>

typedef struct GB GB;

typedef int (*opcode_fn) (GB *gb);

typedef struct {
    	opcode_fn main[256];
    	opcode_fn cb[256];
} OpcodeTable;

void init_opcodes (OpcodeTable *t);

#endif
