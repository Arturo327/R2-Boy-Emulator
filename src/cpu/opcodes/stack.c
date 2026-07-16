#include "cpu/opcodes/common.h"

// --------------------- POP r16 -----------------------

static void pop_bc_l (GB *gb) {
	gb->cpu.c = gb->cpu.bus->read8(gb->bus.ctx, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
static void pop_de_l (GB *gb) {
	gb->cpu.e = gb->cpu.bus->read8(gb->bus.ctx, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
static void pop_hl_l (GB *gb) {
	gb->cpu.l = gb->cpu.bus->read8(gb->bus.ctx, gb->cpu.sp++);
	oam_bug(gb, gb->cpu.sp, 2);
}
static void pop_af_l (GB *gb) {
	gb->cpu.f = gb->cpu.bus->read8(gb->bus.ctx, gb->cpu.sp++) & 0xF0;
	oam_bug(gb, gb->cpu.sp, 2);
}

static void pop_bc_h (GB *gb) {
	gb->cpu.b = read8(gb, gb->cpu.sp++);
}
static void pop_de_h (GB *gb) {
	gb->cpu.d = read8(gb, gb->cpu.sp++);
}
static void pop_hl_h (GB *gb) {
	gb->cpu.h = read8(gb, gb->cpu.sp++);
}
static void pop_af_h (GB *gb) {
	gb->cpu.a = read8(gb, gb->cpu.sp++);
}

#define DEF_POP(name)				\
static void decode_pop_##name (GB *gb) {	\
	push_mcycle(gb, pop_##name##_l);	\
	push_mcycle(gb, pop_##name##_h);	\
}

DEF_POP(bc)	// 0xC1
DEF_POP(de)	// 0xD1
DEF_POP(hl)	// 0xE1
DEF_POP(af)	// 0xF1
 
#undef DEF_POP

// --------------------- PUSH r16 -----------------------

static void push_bc_l (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 1);
	gb->bus.write8(gb->bus.ctx, --gb->cpu.sp, gb->cpu.c);
}
static void push_de_l (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 1);
	gb->bus.write8(gb->bus.ctx, --gb->cpu.sp, gb->cpu.e);
}
static void push_hl_l (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 1);
	gb->bus.write8(gb->bus.ctx, --gb->cpu.sp, gb->cpu.l);
}
static void push_af_l (GB *gb) {
	oam_bug(gb, gb->cpu.sp, 1);
	gb->bus.write8(gb->bus.ctx, --gb->cpu.sp, gb->cpu.f);
}

static void push_bc_h (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.b);
}
static void push_de_h (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.d);
}
static void push_hl_h (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.h);
}
static void push_af_h (GB *gb) {
	write8(gb, gb->cpu.sp, gb->cpu.a);
}

#define DEF_PUSH(name)				\
static void decode_push_##name (GB *gb) {	\
	push_mcycle(gb, dec16_sp);		\
	push_mcycle(gb, push_##name##_h);	\
	push_mcycle(gb, push_##name##_l);	\
}

DEF_PUSH(bc)	// 0xC5
DEF_PUSH(de)	// 0xD5
DEF_PUSH(hl)	// 0xE5
DEF_PUSH(af)	// 0xF5
 
#undef DEF_PUSH

void init_stack (OpcodeTable *t)
{
	t->main[0xC1] = decode_pop_bc;
	t->main[0xC5] = decode_push_bc;
	t->main[0xD1] = decode_pop_de;
	t->main[0xD5] = decode_push_de;
	t->main[0xE1] = decode_pop_hl;
	t->main[0xE5] = decode_push_hl;
	t->main[0xF1] = decode_pop_af;
	t->main[0xF5] = decode_push_af;
}
